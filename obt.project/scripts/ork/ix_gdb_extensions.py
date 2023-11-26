import gdb, re
from obt import deco

decor = deco.Deco()

###############################################################################


class FilterThreads(gdb.Command):
  """Filter and display threads not stopped in __futex_abstimed_wait_common64"""

  def __init__(self):
    super(FilterThreads, self).__init__("ork-threads-filtered",
                                        gdb.COMMAND_USER)

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
      id_str = decor.yellow("%03d" % thread.num)
      na_str = decor.orange(thread_name)
      fr_str = decor.magenta(frame.name())
      st_str = decor.cyan(str(frame.unwind_stop_reason()))
      id_str = "id: %s" % id_str
      na_str = "name: %s" % na_str
      fr_str = "frame: %s" % fr_str
      st_str = "stat: %s" % st_str
      print("THR: %-8s %-60s" % (id_str, na_str))
      print("     %-16s %-60s" % (st_str, fr_str))
      #frame.name(), frame.unwind_stop_reason()))


###############################################################################

class RGB:
  def __init__(self, r, g, b):
    self.r = r
    self.g = g
    self.b = b

class Segment:
  def __init__(self, level, text):
    self.level = level
    self.text = text

###############################################################################


class FilteredBacktrace(gdb.Command):
  """Custom backtrace command that filters out certain std:: frames."""

  def __init__(self):
    super(FilteredBacktrace, self).__init__("ork-bt-filtered",
                                            gdb.COMMAND_STACK)

    self.level_color_map = {
        0: RGB(192, 192, 192),  
        1: RGB(96, 224, 128),  
        2: RGB(96, 255, 224),  
        3: RGB(32, 255, 192),  
        4: RGB(0, 255, 0),  
        5: RGB(255, 255, 255),  
        100: RGB(255, 255, 128),  
    }

  ############################################################

  def create_segments(self,s):
    level = 0
    segments = []
    temp_str = ""
    symbol_level = 100  # Level for <>() symbols

    #####################################

    for char in s:
        if char in "<(":
            if temp_str:
                segments.append(Segment(level, temp_str))
                temp_str = ""
            segments.append(Segment(symbol_level, char))
            level += 1
        elif char in ">)":
            if temp_str:
                segments.append(Segment(level, temp_str))
                temp_str = ""
            segments.append(Segment(symbol_level, char))
            level = max(level - 1, 0)  # Ensure level doesn't go below 0
        else:
            temp_str += char

    #####################################

    if temp_str:
        segments.append(Segment(level, temp_str))

    #####################################

    return segments

  ############################################################
  
  def simplify_stl_containers(self,s):
    # Pattern to match STL containers with default allocator
    pattern = r'(std::\w+)<([^,>]+), std::\w+<[^>]+>>+'

    # Function to replace matched pattern
    def replacer(match):
        container_type = match.group(1)
        contained_type = match.group(2)
        return f"{container_type}<{contained_type}>"

    return re.sub(pattern, replacer, s)
  
  ############################################################

  def replace_std_string(self,s):
    std_string_long = "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >"
    std_string_short = "std::string"
    return s.replace(std_string_long, std_string_short)

  ############################################################

  def colorizeFrameName(self, s):
    result = ""
    s = self.replace_std_string(s)
    s = self.simplify_stl_containers(s)
    segments = self.create_segments(s)
    for segment in segments:
        color = self.level_color_map.get(segment.level, RGB(255, 255, 255))
        colored_text = decor.rgbstr(color.r, color.g, color.b, segment.text)
        result += colored_text
    return result

  ############################################################

  def invoke(self, arg, from_tty):
    frame = gdb.newest_frame()
    while frame:
      sal = frame.find_sal()
      frame_file = sal.symtab.filename if sal.symtab else ""
      if not self.should_filter(frame_file):
        lev_str = decor.yellow("%03d" % frame.level())
        fil_str = decor.orange(frame_file)
        lin_str = decor.magenta("%03d" % sal.line)
        name_str = self.colorizeFrameName(frame.name())
        print("FRAME #: %-8s line: %-4s src: %-60s " %
              (lev_str, lin_str, fil_str))

        print("      %s " % name_str)
        #print(
        #    f"# {frame.level()} in {frame.name()} at {frame_file}:{sal.line}")
      frame = frame.older()

  @staticmethod
  def should_filter(file_name):
    # Define your filtering criteria here
    return "lib/gcc/x86_64-linux-gnu" in file_name


###############################################################################

FilteredBacktrace()
FilterThreads()
