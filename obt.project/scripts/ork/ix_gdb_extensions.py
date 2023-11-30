import gdb, re
import gdb.printing
from parsy import string, seq, regex, Parser, any_char
from obt import deco

decor = deco.Deco()

###############################################################################
# Basic parsers
###############################################################################

# Basic parsers
identifier = regex(r'[a-zA-Z_][a-zA-Z0-9_]*')
namespace = seq(identifier,
                string('::')).combine(lambda ident, colon: ident + colon)
qualified_identifier = seq(namespace.optional(),
                           identifier).map(lambda x: ''.join(filter(None, x)))
angle_bracket_open = string('<')
angle_bracket_close = string('>')
comma = string(',')


# Recursive parser for template arguments
@Parser
def template_arg(input, index):
  content = yield (template | qualified_identifier).sep_by(comma)
  return ",".join(filter(None, content))


# Parser for templates
template = seq(qualified_identifier + angle_bracket_open, template_arg,
               angle_bracket_close).combine(
                   lambda a, b, c: ''.join(filter(None, [a[0], '<', b, '>'])))

# Parser for std::deque, simplifying to keep only the first argument
std_deque = seq(
    string('std::deque<'),
    template_arg.map(lambda args: args.split(',')[0].strip()),
    angle_bracket_close).combine(lambda a, b, c: 'std::deque<' + b + '>')

# Parser for std::queue, handling std::deque with allocator specifically
std_queue = seq(
    string('std::queue<'),
    template_arg.map(lambda args: std_deque.parse(args)
                     if 'std::deque' in args else args.split(',')[0].strip()),
    angle_bracket_close).combine(lambda a, b, c: 'std::queue<' + b + '>')

# General parser that handles any text and std::queue
general_parser = (std_queue | std_deque | any_char.at_least(1).concat()).many()

###############################################################################


class FilterThreads(gdb.Command):
  """Filter and display threads not stopped in __futex_abstimed_wait_common64"""

  def __init__(self):
    super(FilterThreads, self).__init__("ork-threads-filtered",
                                        gdb.COMMAND_USER)

  def invoke(self, arg, from_tty):
    filter_opq_idle = '--no-idle-opq' in arg or '-i' in arg
    sorted_threads = dict()
    thread = gdb.selected_thread()
    for i in gdb.inferiors():
      for t in i.threads():
        t.switch()
        frame = gdb.newest_frame()
        if frame and "__futex_abstimed_wait_common64" not in frame.name():
          show = True
          if filter_opq_idle:
            if frame and '__GI___clock_nanosleep' in frame.name():
              t_name = t.name if t.name else "N/A"
              if t_name == "concurrentQueue" or t_name == "coordinatorSeri":
                frame_count = 1
                while frame:
                  frame = frame.older()
                  frame_count += 1
                if frame_count <= 10:
                  show = False
          if show:
            sorted_threads[t.num] = t
          # Format and print thread information
    for i in sorted(sorted_threads.keys()):
      t = sorted_threads[i]
      t.switch()
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

    self.replace_map = {
        #std::shared_ptr<ork::lev2::Texture>
        "std::shared_ptr<ork::lev2::Texture>": "lev2::texture_ptr_t",
        "std::shared_ptr<ork::DataBlock const>": "datablock_constptr_t",
        "std::shared_ptr<ork::DataBlock>": "datablock_ptr_t",
        "std::shared_ptr<ork::AppInitData>": "appinitdata_ptr_t",
        "std::shared_ptr<ork::lev2::CameraDataLut>":
        "lev2::cameradatalut_ptr_t",
        "std::shared_ptr<ork::lev2::CameraData>": "lev2::cameradata_ptr_t",
        "std::shared_ptr<ork::lev2::scenegraph::Scene>": "lev2SG::scene_ptr_t",
        "ork::Vector2<float>": "fvec2",
        "ork::Vector3<float>": "fvec3",
        "ork::Vector4<float>": "fvec4",
        "ork::Vector2<double>": "dvec2",
        "ork::Vector3<double>": "dvec3",
        "ork::Vector4<double>": "dvec4",
    }

  ############################################################

  def create_segments(self, s):
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

  def simplify_string(self, s):
    std_string_long = "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >"
    std_string_short = "std::string"
    return s.replace(std_string_long, std_string_short)

  ############################################################

  def common_replacements(self, s):
    for k in self.replace_map.keys():
      s = s.replace(k, self.replace_map[k])
    return s

  ############################################################

  def colorizeFrameName(self, s):
    result = ""
    if s == None:
      return ""
    s = self.simplify_string(s)
    s = self.common_replacements(s)
    parsed = general_parser.parse(s)
    s = ''.join(filter(None, parsed))
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
      frame = frame.older()

  @staticmethod
  def should_filter(file_name):
    # Define your filtering criteria here
    do_filter = "lib/gcc/x86_64-linux-gnu" in file_name
    do_filter = do_filter or "conda/python" in file_name
    do_filter = do_filter or "include/pybind11" in file_name
    return do_filter


###############################################################################


class MyStringPrinter:
  """Pretty-printer for std::string."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    # Check if string uses SSO (short string optimization)
    string_length = self.val['_M_string_length']
    capacity = self.val.type.template_argument(0).sizeof * 2 - 1

    if string_length > capacity:
      # Long string, use _M_p
      return self.val['_M_dataplus']['_M_p'].string()
    else:
      # Short string, use _M_local_buf
      return self.val['_M_local_buf'].string(length=string_length)


class RenderContextInstDataPrinter:
  """Pretty-printer for RenderContextInstData."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    # Custom formatting logic
    fields = []
    for field in self.val.type.fields():
      field_val = self.val[field.name]
      fields.append(f"{field.name} = {field_val}")
    r_str = "ork::lev2::RenderContextInstData {\n"
    for item in fields:
      r_str += "  %s\n" % item
    r_str += "}\n"
    return r_str


class TextureDataPrinter:
  """Pretty-printer for Texture."""

  def __init__(self, val):
    # Dereference pointers and std::shared_ptr
    if val.type.code == gdb.TYPE_CODE_PTR:
      self.val = val.referenced_value()
    elif val.type.strip_typedefs(
    ).code == gdb.TYPE_CODE_STRUCT and val.type.tag == 'std::__shared_ptr':
      # Extract the actual object from std::shared_ptr
      self.val = val['_M_ptr'].dereference()
    else:
      self.val = val

  def to_string(self):
    fields = []
    for field in self.val.type.fields():
      field_val = self.val[field.name]
      if field.name != "_impl" and field.name != "_varmap":
        if field.name == "_debugName":
          # Special handling for _debugName field
          debug_name_obj = self.val["_debugName"]
          try:
            # Attempt to call c_str() and retrieve the string value
            field_val = debug_name_obj.dereference()['c_str']()
          except gdb.error:
            # Fallback if c_str() method is not available or fails
            field_val = "ERROR"
        strout = decor.key(field.name)
        strout += " = "
        strout += decor.val(field_val)
        fields.append(strout)
    r_str = "ork::lev2::Texture {\n"
    for item in fields:
      r_str += "  %s\n" % item
    r_str += "}\n"
    return r_str


###############################################################################


class OrkStepInCommand(gdb.Command):
  """Step in and stop at the next line that passes a specified filter. 
    If the line doesn't pass the filter, step out."""

  def __init__(self):
    super(OrkStepInCommand, self).__init__("ork-step-in", gdb.COMMAND_USER)

  def invoke(self, arg, from_tty):
    def source_filter(file_name):
      return "orkid" in file_name

    while True:

      gdb.execute("step", to_string=True)
      frame = gdb.selected_frame()
      if not frame:
        print("No frame available.")
        return

      sal = frame.find_sal()  # Get source and line info
      if not sal or not sal.symtab:
        continue  # No source info available, keep stepping

      filename = sal.symtab.fullname()
      if source_filter(filename):
        # We've reached a line that passes the filter
        print(f"Stopped at {filename}:{sal.line}")
        break
      else:
        # If the filter doesn't pass, step out and continue
        gdb.execute("finish", to_string=True)

###############################################################################

class OrkStepOutCommand(gdb.Command):
  """Step in and stop at the next line that passes a specified filter. 
    If the line doesn't pass the filter, step out."""

  def __init__(self):
    super(OrkStepOutCommand, self).__init__("ork-step-out", gdb.COMMAND_USER)

  def invoke(self, arg, from_tty):
    def source_filter(file_name):
      return "orkid" in file_name

    finished = False
    
    while not finished:
      gdb.execute("finish", to_string=True)
      frame = gdb.selected_frame()
      if not frame:
        print("No frame available.")
        finished = True
        continue
      else:
        sal = frame.find_sal()  # Get source and line info
        if not sal or not sal.symtab:
          continue  # No source info available, keep stepping
        filename = sal.symtab.fullname()
        if source_filter(filename):
          # We've reached a line that passes the filter
          print(f"Stopped at {filename}:{sal.line}")
          finished = True
          break

###############################################################################


def build_pretty_printer():
  pp = gdb.printing.RegexpCollectionPrettyPrinter("MyPrettyPrinters")
  pp.add_printer('ork::lev2::RenderContextInstData', '^RenderContextInstData$',
                 RenderContextInstDataPrinter)
  pp.add_printer('ork::lev2::Texture', '^ork::lev2::Texture$',
                 TextureDataPrinter)
  pp.add_printer('std::string', '^std::(__cxx11::)?string$', MyStringPrinter)
  return pp


###############################################################################

FilteredBacktrace()
FilterThreads()
OrkStepInCommand()
OrkStepOutCommand()

gdb.printing.register_pretty_printer(None,build_pretty_printer())
