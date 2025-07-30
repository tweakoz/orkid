from obt import dep, command, docker, wget, pathtools, host
from obt.deco import Deco
from obt import path as obt_path
from ork import path as ork_path
import obt.module
import time, re, socket, os, sys
from pathlib import Path
deco = obt.deco.Deco()

this_path = os.path.realpath(__file__)
this_dir = Path(os.path.dirname(this_path))

###############################################################################

class dockerinfo:
  ###############################################
  def __init__(self):
    super().__init__()
    self.type = docker.Type.COMPOSITE # use docker-compose
    self._name = "ork-devcdn"
    self._manifest_path = obt_path.manifests_root()/self._name
    # Use ork.path.cdntest as content directory
    self.CONTENT_PATH = str(ork_path.cdntest.resolve())
    self.LOG_PATH = str(obt_path.stage())
  ###############################################
  # build the docker images
  ###############################################
  def build(self, build_args):
    os.chdir(this_dir)
    os.environ["CONTENT_PATH"] = self.CONTENT_PATH
    os.environ["LOGDIR"] = self.LOG_PATH
    print(f"Building ork-devcdn with content path: {self.CONTENT_PATH} log path: {self.LOG_PATH}")
    rval = command.run(["docker", "compose", "build"],do_log=True)
    OK = (rval == 0)
    print(rval, OK)
    return OK
  ###############################################
  # kill active docker containers
  ###############################################
  def kill(self):
    os.chdir(str(this_dir))
    command.run(["docker", "compose", "down"])
  ###############################################
  # launch docker containers
  ###############################################
  def launch(self, launch_args, environment=None, mounts=None):
    os.chdir(this_dir)
    os.environ["CONTENT_PATH"] = self.CONTENT_PATH
    os.environ["LOGDIR"] = self.LOG_PATH
    print("\nLaunching ork-devcdn with content path:", os.environ["CONTENT_PATH"])
    command.run(["docker", "compose", "up"])
  ###############################################
  # show logs
  ###############################################
  def logs(self, service=None):
    os.chdir(this_dir)
    if service:
      command.run(["docker", "compose", "logs", "-f", service])
    else:
      command.run(["docker", "compose", "logs", "-f"])
  ###############################################
  # information dictionary
  ###############################################
  def info(self):
    return {
      "name": "ork-devcdn",
      "description": "development CDN",
      "manifest": str(self._manifest_path),
    }