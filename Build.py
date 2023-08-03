# ChatGPT generated. Command-line build tool, Windows only.
# Looks for the first .uplugin file in the current directory,
# searches for the installed engine, and invokes the 
# RunUAT.bat command to compile the plugin in Build directory.

# Usage: python Build.py -v 5.2

import os
import subprocess
import winreg
import sys
import shutil
import argparse

if sys.platform != "win32":
    print("This build script only work for Windows!")
    sys.exit()

script_dir = os.path.dirname(os.path.abspath(__file__))
build_dir = os.path.join(script_dir, "Build")

# Helper context manager to change and restore the working directory
class change_directory:
    def __init__(self, new_path):
        self.new_path = new_path
        self.prev_path = os.getcwd()

    def __enter__(self):
        os.chdir(self.new_path)

    def __exit__(self, exc_type, exc_val, exc_tb):
        os.chdir(self.prev_path)

def find_first_uplugin_filename():
    current_dir = os.getcwd()

    for filename in os.listdir(current_dir):
        name, ext = os.path.splitext(filename)
        if ext == '.uplugin':
            return name

    return None

def find_unreal_engine_path(engine_version):
    ue_key_name = r"SOFTWARE\EpicGames\Unreal Engine\{}".format(engine_version)
    ue_value_name = "InstalledDirectory"

    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, ue_key_name) as key:
            ue_path, _ = winreg.QueryValueEx(key, ue_value_name)
            uat_path = os.path.join(ue_path, "Engine", "Build", "BatchFiles", "RunUAT.bat")
            if not os.path.exists(uat_path):
                raise FileNotFoundError()
            return ue_path
    except FileNotFoundError:
        print(f"Unreal engine {engine_version} not found.")
        return None

def is_7zip_installed():
    try:
        subprocess.run(["7z", "--help"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return True
    except FileNotFoundError:
        return False

def pack_with_7zip(plugin_name, engine_version):
    zip_filename = os.path.join(build_dir, f"{plugin_name}_ue{engine_version}.zip")
    source_path = os.path.join(build_dir, plugin_name)

    if is_7zip_installed():
        print("7-Zip is installed. Packing files using 7-Zip...")
        with change_directory(f"Build/{engine_version}"):
            subprocess.run(["7z", "a", zip_filename, f"{plugin_name}\\Resources\\", f"{plugin_name}\\Source\\", f"{plugin_name}\{plugin_name}.uplugin"])
        print(f"Files packed into {zip_filename}")
    else:
        print("7-Zip is not installed. Cannot pack files.")

def build_plugin(engine_version):
    plugin_name = find_first_uplugin_filename()
    if not plugin_name:
        print(f"Plugin description file not found...")
        return
    
    des_file = os.path.join(script_dir, f"{plugin_name}.uplugin")

    ue_path = find_unreal_engine_path(engine_version)
    if ue_path:
        print(f"Unreal engine {engine_version} = {ue_path}")
        print(f"Building plugin for Unreal engine {engine_version}...")
        uat_path = os.path.join(ue_path, "Engine", "Build", "BatchFiles", "RunUAT.bat")
        package = os.path.join(build_dir, engine_version, plugin_name)
        subprocess.run([uat_path, "BuildPlugin", f'-Plugin={des_file}', f'-Package={package}'])

        target_dir = os.path.join(script_dir, f"{ue_path}\\Engine\\Plugins\\Marketplace\\{plugin_name}")
        if os.path.exists(target_dir):
            shutil.rmtree(target_dir)
        shutil.copytree(package, target_dir)
        shutil.copy(des_file, target_dir)
        print("Files copied to target plugin directory.")
        pack_with_7zip(plugin_name, engine_version)
    else:
        print(f"Skip build for {engine_version}...")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build and install Unreal Engine plugin.")
    parser.add_argument("--all", action="store_true", help="Build for all supported versions")
    parser.add_argument("--v", type=str, help="Specify the version to build")
    
    args = parser.parse_args()
    
    # Add more versions to this list
    predefined_versions = ["5.2", "5.1", "5.0"]
    if args.all:
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)
        os.makedirs(build_dir)
        for version in predefined_versions:
            build_plugin(version)
    elif args.v:
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)
        os.makedirs(build_dir)
        build_plugin(args.v)
    else:
        parser.print_help()
    
