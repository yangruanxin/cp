"""
端到端测试脚本：编译 ToyC → RISC-V 汇编 → 模拟执行 → 比对返回值

用法:
  python tests/run_tests.py                      # 运行所有测试
  python tests/run_tests.py --test 01_return     # 运行单个测试

环境要求（可选，用于完整验证）：
  - RISC-V GCC 工具链 (riscv32-unknown-elf-gcc 或 riscv64-unknown-elf-gcc)
  - RISC-V 模拟器 (spike + pk, 或 qemu-riscv32)

如果缺少 RISC-V 工具，脚本仅做汇编生成检查（smoke test）。
"""

import subprocess
import sys
import os
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = REPO_ROOT / "src"
TEST_DIR = REPO_ROOT / "tests" / "功能测试用例"
BUILD_DIR = REPO_ROOT / "build"

# 期望的返回值（每个 .tc 文件的预期 exit code）
EXPECTED = {
    "01_return": 42,
    "02_arithmetic": 13,
    "03_control_flow": 10,
    "04_functions_globals": 30,
    "05_logic": 1,
    "06_nested_if_else": 1,
    "07_scope_shadowing": 1,
    "08_boolean_compare": 1,
    "09_continue_and_nested_loops": 5,
    "10_nested_functions": 55,
    "11_global_var": 3,
    "12_unary_ops": 17,
    "13_void_function": 7,
    "14_complex_expr": 8,
}

def find_compiler():
    """找编译器可执行文件"""
    candidates = [
        str(BUILD_DIR / "compiler.exe"),
        str(BUILD_DIR / "compiler"),
        str(BUILD_DIR / "Debug" / "compiler.exe"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None

def find_riscv_tools():
    """找 RISC-V 工具链"""
    gcc_candidates = ["riscv32-unknown-elf-gcc", "riscv64-unknown-elf-gcc"]
    sim_candidates = ["spike", "qemu-riscv32", "qemu-riscv64"]

    gcc = None
    for c in gcc_candidates:
        if subprocess.run(["where", c], capture_output=True).returncode == 0:
            gcc = c
            break

    sim = None
    for c in sim_candidates:
        if subprocess.run(["where", c], capture_output=True).returncode == 0:
            sim = c
            break

    pk = None
    if subprocess.run(["where", "pk"], capture_output=True).returncode == 0:
        pk = "pk"

    return gcc, sim, pk


def compile_tc(tc_path, compiler):
    """编译 .tc 文件为 .s 汇编"""
    s_path = tc_path.with_suffix(".s")
    try:
        with open(tc_path) as f:
            src = f.read()
        result = subprocess.run(
            [compiler],
            input=src,
            capture_output=True,
            text=True,
            timeout=30
        )
        if result.returncode != 0:
            return False, result.stderr, None
        with open(s_path, "w") as f:
            f.write(result.stdout)
        return True, result.stdout, s_path
    except Exception as e:
        return False, str(e), None


def run_assembly(s_path, gcc, sim):
    """用 RISC-V 工具链运行汇编并获取返回值"""
    elf_path = s_path.with_suffix(".elf")
    try:
        # 汇编 + 链接
        subprocess.run(
            [gcc, "-march=rv32im", "-mabi=ilp32", "-nostdlib",
             "-Wl,-e,main", "-o", str(elf_path), str(s_path)],
            capture_output=True, text=True, timeout=30, check=True
        )

        # 模拟执行
        if sim == "spike":
            result = subprocess.run(
                [sim, "pk", str(elf_path)],
                capture_output=True, text=True, timeout=30
            )
        elif sim and "qemu" in sim:
            result = subprocess.run(
                [sim, str(elf_path)],
                capture_output=True, text=True, timeout=30
            )
        else:
            return None, "No simulator available"

        # 返回码 = exit code
        return result.returncode, None
    except subprocess.CalledProcessError as e:
        return None, f"Link/run failed: {e.stderr}"
    except Exception as e:
        return None, str(e)


def main():
    compiler = find_compiler()
    if not compiler:
        print("错误：未找到编译器可执行文件。请先执行 cmake --build .")
        sys.exit(1)

    gcc, sim, pk = find_riscv_tools()
    has_full_test = gcc and sim

    if has_full_test:
        print(f"RISC-V 工具链: GCC={gcc}, Sim={sim}")
    else:
        print("RISC-V 工具链未找到，仅执行汇编生成检查 (smoke test)")

    # 选择测试用例
    if len(sys.argv) > 2 and sys.argv[1] == "--test":
        selected = [sys.argv[2]]
    else:
        selected = [f.stem for f in sorted(TEST_DIR.glob("*.tc"))]

    passed = 0
    failed = 0

    for name in selected:
        tc_path = TEST_DIR / f"{name}.tc"
        if not tc_path.exists():
            print(f"  ⚠  跳过: {name} (文件不存在)")
            continue

        expected = EXPECTED.get(name)
        print(f"\n  [{name}] 期望返回值 = {expected}")

        # 编译
        ok, output, s_path = compile_tc(tc_path, compiler)
        if not ok:
            print(f"  ✗ 编译失败: {output}")
            failed += 1
            continue

        print(f"  ✓ 汇编生成 ({s_path.name})")

        if has_full_test:
            # 运行并验证
            retcode, err = run_assembly(s_path, gcc, sim)
            if err:
                print(f"  ? 运行出错: {err}")
                failed += 1
            elif retcode == expected:
                print(f"  ✓ 返回值 = {retcode} ✓")
                passed += 1
            else:
                print(f"  ✗ 返回值 = {retcode} (期望 {expected})")
                failed += 1
        else:
            # 仅 smoke test: 检查汇编文件非空
            asm_text = open(s_path).read()
            if asm_text.strip() and ".text" in asm_text:
                print(f"  ✓ Smoke test 通过")
                passed += 1
            else:
                print(f"  ✗ 汇编文件似乎无效")
                failed += 1

    print(f"\n{'='*40}")
    print(f"结果: {passed} 通过, {failed} 失败 / {passed + failed} 总计")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
