Fuzzing
=======

Reproducing using `fuzz`
------------------------

We assume that you can [build Skia](/user/build). Many fuzzes only reproduce
when building with ASAN or MSAN; see [those instructions for more details](./xsan).

When building, you should add the following args to BUILD.gn to make reproducing
less machine- and platform- dependent:

    skia_use_fontconfig=false
    skia_use_freetype=true
    skia_use_system_freetype2=false
    skia_use_wuffs=true
    skia_enable_skottie=true
    skia_enable_fontmgr_custom_directory=false
    skia_enable_fontmgr_custom_embedded=false
    skia_enable_fontmgr_custom_empty=true

All that is needed to reproduce a fuzz downloaded from ClusterFuzz, oss-fuzz or
fuzzer.skia.org is to run something like:

    out/ASAN/fuzz -b /path/to/downloaded/testcase

The fuzz binary will try its best to guess what the type/name should be based on
the name of the testcase. Manually providing type and name is also supported, like:

    out/ASAN/fuzz -t filter_fuzz -b /path/to/downloaded/testcase
    out/ASAN/fuzz -t api -n RasterN32Canvas -b /path/to/downloaded/testcase

To enumerate all supported types and names, run the following:

    out/ASAN/fuzz --help  # will list all types
    out/ASAN/fuzz -t api  # will list all names

If the crash does not show up, try to add the flag --loops:

    out/ASAN/fuzz -b /path/to/downloaded/testcase --loops <times-to-run>

Writing fuzzers with libfuzzer
------------------------------

libfuzzer is an easy way to write new fuzzers, and how we run them on oss-fuzz.
Your fuzzer entry point should implement this API:

    extern "C" int LLVMFuzzerTestOneInput(const uint8_t*, size_t);

First install Clang and libfuzzer, e.g.

    sudo apt install clang-10 libc++-10-dev libfuzzer-10-dev

You should now be able to use `-fsanitize=fuzzer` with Clang.

Set up GN args to use libfuzzer:

    cc = "clang-10"
    cxx = "clang++-10"
    sanitize = "fuzzer"
    extra_cflags = [ "-O1" ]  # Or whatever you want.
    ...

Build Skia and your fuzzer entry point:

    ninja -C out/libfuzzer skia
    clang++-10 -I. -O1 -fsanitize=fuzzer fuzz/oss_fuzz/whatever.cpp out/libfuzzer/libskia.a

Run your new fuzzer binary

    ./a.out
