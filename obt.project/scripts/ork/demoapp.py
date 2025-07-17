import argparse, signal


def parser(description='orkid example'):
  parser = argparse.ArgumentParser(description=description)
  parser.add_argument('--newlogger', action='store_true', help='new logger mode')    
  return parser



def install_signal_handler(ezapp):
  def onCtrlC(signum, frame):
    print("signalling EXIT to ezapp")
    ezapp.signalExit()
  signal.signal(signal.SIGINT, onCtrlC)
