# Examples of using simpleperf to profile android applications

## Table of Contents

- [Introduction](#introduction)
- [Profiling java application](#profiling-java-application)
- [Profiling java/c++ application](#profiling-javac-application)

## Introduction

Simpleperf is a native profiler used on Android platform. It can be used to profile android
applications. It's document is at [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/README.md).
This directory is to show examples of using simpleperf to profile android applications. The
meaning of each directory is as below:

    ../scripts/                  -- contain simpleperf binaries and scripts.
    SimpleperfExamplePureJava/   -- contains an android studio project using only java code.
    SimpleperfExampleWithNative/ -- contains an android studio project using both java/c++ code.

It can be downloaded as below:

    $git clone https://android.googlesource.com/platform/system/extras
    $cd extras/simpleperf/demo

## Profiling java application

    android studio project: SimpleExamplePureJava
    test device: Android O (Google Pixel XL)
    test device: Android N (Google Nexus 5X)

steps:
1. Build and install app:

    # Open SimpleperfExamplesPureJava project with Android studio,
    # and build this project sucessfully, otherwise the `./gradlew` command below will fail.
    $cd SimpleperfExamplePureJava
    $./gradlew clean assemble
    $adb install -r app/build/outputs/apk/app-profiling.apk

2. Record profiling data:

    $cd ../../scripts/
    $gvim app_profiler.config
      change app_package_name line to: app_package_name = "com.example.simpleperf.simpleperfexamplepurejava"
    $python app_profiler.py
      It runs the application and collects profiling data in perf.data, binaries on device in binary_cache/.

3. Show profiling data:

    a. show call graph in txt mode
        $bin/linux/x86_64/simpleperf report -g --brief-callgraph | more
          If on other hosts, use corresponding simpleperf binary.
    b. show call graph in gui mode
        $python report.py -g
    c. show samples in source code
        $gvim annotate.config
          change source_dirs line to: source_dirs = ["../demo/SimpleperfExamplePureJava"]
        $python annotate.py
        $gvim annotated_files/java/com/example/simpleperf/simpleperfexamplepurejava/MainActivity.java
          check the annoated source file MainActivity.java.

## Profiling java/c++ application

    android studio project: SimpleExampleWithNative
    test device: Android O (Google Pixel XL)
    test device: Android N (Google Nexus 5X)

steps:
1. Build and install app:

    # Open SimpleperfExamplesPureJava project with Android studio,
    # and build this project sucessfully, otherwise the `./gradlew` command below will fail.
    $cd SimpleperfExampleWithNative
    $./gradlew clean assemble
    $adb install -r app/build/outputs/apk/app-profiling.apk

2. Record profiling data:

    $cd ../../scripts/
    $gvim app_profiler.config
      change app_package_name line to: app_package_name = "com.example.simpleperf.simpleperfexamplewithnative"
    $python app_profiler.py
      It runs the application and collects profiling data in perf.data, binaries on device in binary_cache/.

3. Show profiling data:

    a. show call graph in txt mode
        $bin/linux/x86_64/simpleperf report -g --brief-callgraph | more
          If on other hosts, use corresponding simpleperf binary.
    b. show call graph in gui mode
        $python report.py -g
    c. show samples in source code
        $gvim annotate.config
          change source_dirs line to: source_dirs = ["../demo/SimpleperfExampleWithNative"]
        $python annotate.py
        $find . -name "native-lib.cpp" | xargs gvim
          check the annoated source file native-lib.cpp.

