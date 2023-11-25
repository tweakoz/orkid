import gdb


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
    gdb.execute("thread " + str(thread.num))
    frame = gdb.newest_frame()
    if frame:
      print("Thread ID: {}, Function: {}, Frame: {}".format(
          thread.num, frame.name(), frame.unwind_stop_reason()))


FilterThreads()
