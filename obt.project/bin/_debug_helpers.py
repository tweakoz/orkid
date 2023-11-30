import os, re

####################################################################

def find_executable(exec_name):
  """Find the executable in PATH or use the provided path."""
  exec_name = os.path.expandvars(exec_name)
  if exec_name.startswith(("./", "../")) or os.path.isabs(exec_name):
    full_path = os.path.abspath(exec_name)
    if os.path.exists(full_path) and os.access(full_path, os.X_OK):
      return full_path
    return None
  for path in os.environ["PATH"].split(os.pathsep):
    full_path = os.path.join(path, exec_name)
    if os.path.exists(full_path) and os.access(full_path, os.X_OK):
      return full_path
  if os.path.exists(exec_name) and os.access(exec_name, os.X_OK):
    return exec_name
  return None

####################################################################

def is_python_script(executable_path):
  """Determine if the given file is a Python script."""
  try:
    with open(executable_path, 'r') as f:
      first_line = f.readline().strip()
      return first_line.startswith("#!") and "python" in first_line
  except Exception as e:
    return False

####################################################################

def get_exec_and_args(parse_args):
  executable_path = find_executable(parse_args.executable_name)

  if not executable_path:
    print(f"Executable '{parse_args.executable_name}' not found in $PATH.")
    exit(1)

  exec_args = parse_args.exec_args

  if is_python_script(executable_path):
    python_path = find_executable("python3")
    if not python_path:
      print("python3 not found.")
      exit(1)
    exec_args.insert(0, executable_path)
    executable_path = os.path.realpath(python_path)

  exec_name = re.sub(r"[^a-zA-Z0-9]", "_", parse_args.executable_name)
  return executable_path, exec_args, exec_name
