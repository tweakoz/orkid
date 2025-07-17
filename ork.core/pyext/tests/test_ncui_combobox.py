#!/usr/bin/env python3

from orkengine.core import Logger, vec3, ncui, CrcStringProxy
import time

# Create tokens proxy
tokens = CrcStringProxy()

logger = Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("COMBO_TEST", vec3(0, 1, 0.5), True)

uictx = ncui.context()
tabg = uictx.content

# Helper function to create test comboboxes
def create_combo(items, selected_index=-1, colors=None):
    combo = ncui.ComboBox()
    combo.items = items
    combo.selected_index = selected_index
    combo.width = 25
    combo.height = 1
    
    if colors:
        combo.bg_color = colors.get('bg', vec3(0.2, 0.2, 0.2))
        combo.fg_color = colors.get('fg', vec3(1, 1, 1))
        combo.selected_bg_color = colors.get('selected_bg', vec3(0.1, 0.3, 0.6))
        combo.selected_fg_color = colors.get('selected_fg', vec3(1, 1, 1))
        combo.dropdown_bg_color = colors.get('dropdown_bg', vec3(0.25, 0.25, 0.25))
        combo.dropdown_fg_color = colors.get('dropdown_fg', vec3(1, 1, 1))
        combo.hover_bg_color = colors.get('hover_bg', vec3(0.3, 0.3, 0.3))
        combo.hover_fg_color = colors.get('hover_fg', vec3(1, 1, 1))
    
    return combo

# Test 1: Basic ComboBox with selection callback
basic_pack = ncui.VerticalPack()

# Simple fruit selection
fruit_combo = create_combo([
    "Apple", "Banana", "Cherry", "Date", "Elderberry", "Fig", "Grape"
], selected_index=0)

def on_fruit_selected(index, value):
    chan_test.log(f"Fruit selected: {value} (index {index})")

fruit_combo.on_selection_changed = on_fruit_selected
basic_pack.addChild(fruit_combo)

# Color selection with custom colors
color_combo = create_combo([
    "Red", "Green", "Blue", "Yellow", "Purple", "Orange", "Pink"
], selected_index=2, colors={
    'bg': vec3(0.1, 0.1, 0.3),
    'selected_bg': vec3(0.2, 0.2, 0.6),
    'dropdown_bg': vec3(0.15, 0.15, 0.4)
})

def on_color_selected(index, value):
    chan_test.log(f"Color selected: {value}")

color_combo.on_selection_changed = on_color_selected
basic_pack.addChild(color_combo)

# Empty combo (for testing)
empty_combo = create_combo([])
empty_combo.on_selection_changed = lambda i, v: chan_test.log("Empty combo somehow selected?")
basic_pack.addChild(empty_combo)

tabg.addTab("Basic", basic_pack)

# Test 2: Long Lists and Scrolling
scroll_pack = ncui.VerticalPack()

# Countries list
countries = [
    "Afghanistan", "Albania", "Algeria", "Argentina", "Australia", "Austria",
    "Bangladesh", "Belgium", "Brazil", "Bulgaria", "Canada", "Chile", "China",
    "Colombia", "Croatia", "Czech Republic", "Denmark", "Egypt", "Finland",
    "France", "Germany", "Greece", "Hungary", "Iceland", "India", "Indonesia",
    "Iran", "Ireland", "Israel", "Italy", "Japan", "Jordan", "Kazakhstan",
    "Kenya", "South Korea", "Latvia", "Lebanon", "Lithuania", "Malaysia",
    "Mexico", "Morocco", "Netherlands", "New Zealand", "Nigeria", "Norway",
    "Pakistan", "Peru", "Philippines", "Poland", "Portugal", "Romania",
    "Russia", "Saudi Arabia", "Singapore", "South Africa", "Spain", "Sweden",
    "Switzerland", "Thailand", "Turkey", "Ukraine", "United Kingdom",
    "United States", "Vietnam"
]

country_combo = create_combo(countries, selected_index=12)  # China
country_combo.max_visible_items = 6
country_combo.on_selection_changed = lambda i, v: chan_test.log(f"Country: {v}")
scroll_pack.addChild(country_combo)

# Numbers 1-100
numbers = [f"Number {i}" for i in range(1, 101)]
number_combo = create_combo(numbers, selected_index=49)  # Number 50
number_combo.max_visible_items = 8
number_combo.on_selection_changed = lambda i, v: chan_test.log(f"Selected: {v}")
scroll_pack.addChild(number_combo)

tabg.addTab("Scrolling", scroll_pack)

# Test 3: Dynamic Content Management
dynamic_pack = ncui.VerticalPack()

# Dynamic list that can be modified
dynamic_items = ["Initial Item 1", "Initial Item 2", "Initial Item 3"]
dynamic_combo = create_combo(dynamic_items.copy(), selected_index=0)

def on_dynamic_selected(index, value):
    chan_test.log(f"Dynamic selection: {value}")

dynamic_combo.on_selection_changed = on_dynamic_selected
dynamic_pack.addChild(dynamic_combo)

# Add item button
add_btn = ncui.Button()
add_btn.text = "Add Item"
add_btn.width = 15
add_btn.height = 2
add_btn.normal_bg_color = vec3(0.1, 0.4, 0.1)

def add_item():
    new_item = f"New Item {len(dynamic_items) + 1}"
    dynamic_items.append(new_item)
    dynamic_combo.items = dynamic_items.copy()
    chan_test.log(f"Added: {new_item}")

add_btn.on_click = add_item
dynamic_pack.addChild(add_btn)

# Remove item button
remove_btn = ncui.Button()
remove_btn.text = "Remove Last"
remove_btn.width = 15
remove_btn.height = 2
remove_btn.normal_bg_color = vec3(0.4, 0.1, 0.1)

def remove_item():
    if dynamic_items:
        removed = dynamic_items.pop()
        dynamic_combo.items = dynamic_items.copy()
        if dynamic_combo.selected_index >= len(dynamic_items):
            dynamic_combo.selected_index = len(dynamic_items) - 1 if dynamic_items else -1
        chan_test.log(f"Removed: {removed}")

remove_btn.on_click = remove_item
dynamic_pack.addChild(remove_btn)

# Clear all button
clear_btn = ncui.Button()
clear_btn.text = "Clear All"
clear_btn.width = 15
clear_btn.height = 2
clear_btn.normal_bg_color = vec3(0.3, 0.3, 0.1)

def clear_all():
    dynamic_items.clear()
    dynamic_combo.clearItems()
    chan_test.log("Cleared all items")

clear_btn.on_click = clear_all
dynamic_pack.addChild(clear_btn)

tabg.addTab("Dynamic", dynamic_pack)

# Test 4: Different Sizes and Configurations
sizes_pack = ncui.VerticalPack()

# Small combo
small_combo = create_combo(["S1", "S2", "S3"], selected_index=0)
small_combo.width = 10
small_combo.max_visible_items = 2
small_combo.on_selection_changed = lambda i, v: chan_test.log(f"Small: {v}")
sizes_pack.addChild(small_combo)

# Medium combo
medium_combo = create_combo(["Medium Option 1", "Medium Option 2", "Medium Option 3"], selected_index=1)
medium_combo.width = 20
medium_combo.max_visible_items = 3
medium_combo.on_selection_changed = lambda i, v: chan_test.log(f"Medium: {v}")
sizes_pack.addChild(medium_combo)

# Large combo
large_combo = create_combo([
    "Very Long Option Name That Should Be Truncated",
    "Another Very Long Option That Tests Text Truncation",
    "Short", "Medium Length Option", "Extra Long Option With Many Words"
], selected_index=2)
large_combo.width = 35
large_combo.max_visible_items = 4
large_combo.on_selection_changed = lambda i, v: chan_test.log(f"Large: {v}")
sizes_pack.addChild(large_combo)

tabg.addTab("Sizes", sizes_pack)

# Test 5: Integrated Example - Configuration Panel
config_pack = ncui.VerticalPack()

# Configuration state
config = {
    'resolution': 0,
    'quality': 1,
    'language': 0,
    'theme': 0
}

# Resolution combo
res_combo = create_combo([
    "1920x1080", "2560x1440", "3840x2160", "1366x768", "1600x900"
], selected_index=config['resolution'])

def on_resolution_changed(index, value):
    config['resolution'] = index
    chan_test.log(f"Resolution set to: {value}")

res_combo.on_selection_changed = on_resolution_changed
config_pack.addChild(res_combo)

# Quality combo
quality_combo = create_combo([
    "Low", "Medium", "High", "Ultra"
], selected_index=config['quality'], colors={
    'bg': vec3(0.2, 0.1, 0.1),
    'selected_bg': vec3(0.4, 0.2, 0.2)
})

def on_quality_changed(index, value):
    config['quality'] = index
    chan_test.log(f"Quality set to: {value}")

quality_combo.on_selection_changed = on_quality_changed
config_pack.addChild(quality_combo)

# Language combo
lang_combo = create_combo([
    "English", "Spanish", "French", "German", "Japanese", "Chinese"
], selected_index=config['language'])

def on_language_changed(index, value):
    config['language'] = index
    chan_test.log(f"Language set to: {value}")

lang_combo.on_selection_changed = on_language_changed
config_pack.addChild(lang_combo)

# Theme combo
theme_combo = create_combo([
    "Light", "Dark", "Blue", "Green"
], selected_index=config['theme'], colors={
    'bg': vec3(0.1, 0.1, 0.2),
    'selected_bg': vec3(0.2, 0.2, 0.4)
})

def on_theme_changed(index, value):
    config['theme'] = index
    chan_test.log(f"Theme set to: {value}")

theme_combo.on_selection_changed = on_theme_changed
config_pack.addChild(theme_combo)

# Apply settings button
apply_btn = ncui.Button()
apply_btn.text = "Apply Settings"
apply_btn.width = 20
apply_btn.height = 3
apply_btn.normal_bg_color = vec3(0.1, 0.3, 0.1)

def apply_settings():
    settings_str = f"Resolution: {res_combo.selected_item}, Quality: {quality_combo.selected_item}, Language: {lang_combo.selected_item}, Theme: {theme_combo.selected_item}"
    chan_test.log(f"Settings applied: {settings_str}")

apply_btn.on_click = apply_settings
config_pack.addChild(apply_btn)

tabg.addTab("Config", config_pack)

# Test 6: Layout Integration
layout_test = ncui.HorizSplit()
layout_test.split_position = 0.6

# Left side: Multiple combos in vertical layout
left_pack = ncui.VerticalPack()

combos_data = [
    (["Option A", "Option B", "Option C"], "Combo 1"),
    (["Choice X", "Choice Y", "Choice Z"], "Combo 2"),
    (["Item 1", "Item 2", "Item 3", "Item 4"], "Combo 3")
]

for items, name in combos_data:
    combo = create_combo(items, selected_index=0)
    combo.on_selection_changed = lambda i, v, n=name: chan_test.log(f"{n}: {v}")
    left_pack.addChild(combo)

# Right side: Mixed content with combo
right_pack = ncui.VerticalPack()

# Text display
text_display = ncui.TextLines()
text_display.color = vec3(1, 1, 0)
text_display.height = 5
text_display.addLine("ComboBox Integration Test")
text_display.addLine("Select items from the left panel")
text_display.addLine("Messages will appear in the log")
right_pack.addChild(text_display)

# Status combo
status_combo = create_combo([
    "Ready", "Processing", "Complete", "Error"
], selected_index=0)
status_combo.on_selection_changed = lambda i, v: chan_test.log(f"Status: {v}")
right_pack.addChild(status_combo)

layout_test.left = left_pack
layout_test.right = right_pack

tabg.addTab("Layout", layout_test)

# Log initial test information
chan_test.log("ComboBox Widget Test Suite")
chan_test.log("- Basic: Simple selection with callbacks")
chan_test.log("- Scrolling: Long lists with scroll functionality")
chan_test.log("- Dynamic: Runtime item management")
chan_test.log("- Sizes: Different sizes and configurations")
chan_test.log("- Config: Configuration panel example")
chan_test.log("- Layout: Integration with other widgets")
chan_test.log("Click dropdowns to test functionality!")
chan_test.log("Use mouse wheel or arrow keys to navigate")

# Status updates
chan_test.status("ComboBoxes", "6 test categories")
chan_test.status("Features", "selection, scrolling, dynamic")
chan_test.status("Interaction", "mouse, keyboard, dropdown")

uictx.waitForExit()

for name in list(locals().keys()):
    if not name.startswith('_'):
        del locals()[name]
