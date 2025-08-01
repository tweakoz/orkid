from obt import command
import re

class DisplayMode:
    def __init__(self, resolution, refresh_rate, color_depth, scaling=False):
        self.resolution = resolution
        self.refresh_rate = refresh_rate
        self.color_depth = color_depth
        self.scaling = scaling
        
    def __repr__(self):
        return f"DisplayMode({self.resolution}, {self.refresh_rate}Hz, {self.color_depth}bit, scaling={self.scaling})"

class Display:
    def __init__(self, id, current_mode, modes, origin="(0,0)", rotation=0, enabled=True):
        self.id = id
        self.current_mode = current_mode
        self.modes = modes
        self.origin = origin
        self.rotation = rotation
        self.enabled = enabled
        
    def hasMode(self, w, h, r, d):
        """Check if display supports a specific mode (width, height, refresh_rate, depth)"""
        target_resolution = f"{w}x{h}"
        for mode in self.modes:
            if (mode.resolution == target_resolution and 
                mode.refresh_rate == r and 
                mode.color_depth == d):
                return True
        return False
        
    def __getitem__(self, key):
        # Support dict-like access for backwards compatibility
        if key == 'id':
            return self.id
        raise KeyError(key)

class DisplayList(list):
    """A list subclass that adds the setDisplayConfigurations method"""
    def setDisplayConfigurations(self, display_configs, do_log=False):
        """Set display configurations using displayplacer with full parameters"""
        if not display_configs:
            if do_log:
                print("No display configurations to set")
            return ""
        
        commands = []
        for config in display_configs:
            cmd_parts = [
                f"id:{config['id']}",
                f"res:{config['resolution']}",
                f"hz:{config['hz']}",
                f"color_depth:{config['color_depth']}",
                f"enabled:{config['enabled']}",
                f"scaling:{config['scaling']}",
                f"origin:{config['origin']}",
                f"degree:{config['rotation']}"
            ]
            commands.append(' '.join(cmd_parts))
            
            if do_log:
                print(f"Configuring display {config['id']}: enabled={config['enabled']}, res={config['resolution']}")
        
        if commands:
            cmd = ["displayplacer"] + commands
            if do_log:
                print(f"Executing: {' '.join(cmd)}")
            result = command.capture(cmd, do_log=do_log)
            return result
        
        return ""

class DisplayPlacer:
    def __init__(self):
        pass
    
    def parse_displayplacer_output(self, output):
        """Parse displayplacer list output into Display objects"""
        displays = []
        lines = output.strip().split('\n')
        
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            
            # Look for display start (Persistent screen id)
            if line.startswith("Persistent screen id:"):
                display_id = line.split(": ")[1]
                
                # Parse current display properties
                current_resolution = None
                current_hz = None
                current_depth = None
                current_scaling = False
                origin = "(0,0)"
                rotation = 0
                enabled = True
                
                # Advance to find current properties
                i += 1
                while i < len(lines) and not lines[i].strip().startswith("Resolutions for rotation"):
                    line = lines[i].strip()
                    if line.startswith("Resolution:"):
                        current_resolution = line.split(": ")[1]
                    elif line.startswith("Hertz:"):
                        current_hz = float(line.split(": ")[1])
                    elif line.startswith("Color Depth:"):
                        current_depth = int(line.split(": ")[1])
                    elif line.startswith("Scaling:"):
                        current_scaling = line.split(": ")[1] == "on"
                    elif line.startswith("Origin:"):
                        origin = line.split(": ")[1].split(" ")[0]  # Get just the coordinates
                    elif line.startswith("Rotation:"):
                        rotation = int(line.split(": ")[1])
                    elif line.startswith("Enabled:"):
                        enabled = line.split(": ")[1].lower() == "true"
                    i += 1
                
                # Create current mode
                current_mode = DisplayMode(current_resolution, current_hz, current_depth, current_scaling)
                
                # Parse available modes
                modes = []
                if i < len(lines) and lines[i].strip().startswith("Resolutions for rotation"):
                    i += 1  # Skip the "Resolutions for rotation" line
                    
                    while i < len(lines):
                        line = lines[i].strip()
                        if not line or line.startswith("Persistent screen id:") or line.startswith("Execute the command"):
                            break
                            
                        # Parse mode line: "  mode 0: res:1280x1024 hz:60 color_depth:8 <-- current mode"
                        if line.startswith("mode "):
                            mode_match = re.search(r'res:(\d+x\d+)\s+hz:(\d+(?:\.\d+)?)\s+color_depth:(\d+)', line)
                            if mode_match:
                                resolution = mode_match.group(1)
                                hz = float(mode_match.group(2))
                                depth = int(mode_match.group(3))
                                scaling = "scaling:on" in line
                                
                                modes.append(DisplayMode(resolution, hz, depth, scaling))
                        
                        i += 1
                
                displays.append(Display(display_id, current_mode, modes, origin, rotation, enabled))
            else:
                i += 1
        
        return displays
    
    def getDisplays(self):
        """Get all displays and return them as a DisplayList"""
        rval_output = command.capture(["displayplacer", "list"], do_log=True)
        displays = self.parse_displayplacer_output(rval_output)
        # Return as DisplayList instead of regular list
        display_list = DisplayList(displays)
        return display_list

# Create a module-level instance and expose its getDisplays method
_displayplacer = DisplayPlacer()
getDisplays = _displayplacer.getDisplays