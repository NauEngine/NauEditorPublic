import os
import sys

preset = 'win_vs2022_x64_dll'
nau_engine_sdk = sys.argv[1]
nau_editor_bin = sys.argv[2]
config = sys.argv[3]
toolchain = sys.argv[4]

def do_engine_build() :
    print("Building NauEngineSDK", preset, config, nau_engine_sdk, nau_editor_bin, flush=True)

    result = os.system("cmake --preset " + preset + " -DNAU_CORE_SAMPLES=OFF -DNAU_CORE_TESTS=OFF -DCMAKE_TOOLCHAIN_FILE=" + toolchain)
    if result != 0 :
        print("NauEngineSDK config failed with code", result, flush=True)
        sys.exit(result)

    result = os.system("cmake --build build\\"+preset+" --parallel --config="+config)
    if result != 0 :
        print("NauEngineSDK build failed with code", result, flush=True)
        sys.exit(result)

    result = os.system("cmake --install build\\"+preset+" --config="+config)
    if result != 0 :
        print("NauEngineSDK install failed with code", result, flush=True)
        sys.exit(result)
    return 0

cwd = os.getcwd()
os.chdir(sys.argv[1])
print("Building NauEngineSDK in ", os.getcwd(), flush=True)

result = do_engine_build() 
result = 0
print("NauEngineSDK build return ", result, flush=True)

os.chdir(cwd)
print ("Back to ", os.getcwd(), flush=True)

if result == 0:
    xcmd = "xcopy.exe /E /I /Y " + nau_engine_sdk + "\\output" + " " + nau_editor_bin + "\\NauEngineSDK"
    print(xcmd, flush=True)
    result = os.system(xcmd)

sys.exit(result)
