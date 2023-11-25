import gdb
from obt import deco
deco = deco.Deco()

class FilterThreads(gdb.Command):
  """Filter and display threads not stopped in __futex_abstimed_wait_common64"""

  def __init__(self):
    super(FilterThreads, self).__init__("threads-filtered", gdb.COMMAND_USER)

  def invoke(self, arg, from_tty):
    thread = gdb.selected_thread()
    for i in gdb.inferiors():
      for t in i.threads():
        t.switch()
        frame = gdb.newest_frame()
        if frame and "__futex_abstimed_wait_common64" not in frame.name():
          # Format and print thread information
          self.print_thread_info(t)

    # Switch back to the original thread
    if thread:
      thread.switch()

  def print_thread_info(self, thread):
    # You can adjust the details you want to print about each thread
    gdb.execute("thread " + str(thread.num), to_string=True)
    frame = gdb.newest_frame()
    if frame:
      thread_name = thread.name if thread.name else "N/A"
      id_str = deco.yellow("%03d"%thread.num)
      na_str = deco.orange(thread_name)
      fr_str = deco.magenta(frame.name())
      st_str = deco.cyan(str(frame.unwind_stop_reason()))
      id_str = "id: %s"%id_str
      na_str = "name: %s"%na_str
      fr_str = "frame: %s"%fr_str
      st_str = "stat: %s"%st_str
      print("THR: %-8s %-60s"%(id_str, na_str))
      print("     %-16s %-60s"%(st_str, fr_str))
      #frame.name(), frame.unwind_stop_reason()))


FilterThreads()
