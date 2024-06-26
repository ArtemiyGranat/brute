from timeit import timeit
from hypothesis import given, strategies as st, settings
from utils import gen_str, brute_cmd, CPU_COUNT


@given(st.text(min_size=1, max_size=1, alphabet="iry"))
@settings(deadline=None)
def test_performance_gen(brute_mode):
    alph = gen_str(12)
    passwd = gen_str(5, alph)

    single_brute = brute_cmd(passwd, alph, "d", brute_mode)
    gen_brute = brute_cmd(passwd, alph, "g", brute_mode)

    single_time = timeit(
        stmt=f"subprocess.check_output('{single_brute}', shell=True)",
        setup="import subprocess",
        number=1,
    )
    gen_time = timeit(
        stmt=f"subprocess.check_output('{gen_brute}', shell=True)",
        setup="import subprocess",
        number=1,
    )
    ratio = single_time / gen_time

    assert ratio >= CPU_COUNT * 0.7


@given(st.text(min_size=1, max_size=1, alphabet="iry"))
@settings(deadline=None)
def test_performance_multi(brute_mode):
    alph = gen_str(12)
    passwd = gen_str(5, alph)

    single_brute = brute_cmd(passwd, alph, "d", brute_mode)
    multi_brute = brute_cmd(passwd, alph, "m", brute_mode)

    single_time = timeit(
        stmt=f"subprocess.check_output('{single_brute}', shell=True)",
        setup="import subprocess",
        number=1,
    )
    multi_time = timeit(
        stmt=f"subprocess.check_output('{multi_brute}', shell=True)",
        setup="import subprocess",
        number=1,
    )
    ratio = single_time / multi_time

    assert ratio >= CPU_COUNT * 0.7
