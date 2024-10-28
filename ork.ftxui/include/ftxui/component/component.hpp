// Copyright 2021 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_COMPONENT_HPP
#define FTXUI_COMPONENT_HPP

#include <functional>  // for function
#include <memory>      // for make_shared, shared_ptr
#include <string>      // for wstring
#include <utility>     // for forward
#include <vector>      // for vector

#include <ftxui/component/component_base.hpp>  // for Component, Components
#include <ftxui/component/component_options.hpp>  // for ButtonOption, CheckboxOption, MenuOption
#include <ftxui/dom/elements.hpp>  // for Element
#include <ftxui/util/ref.hpp>  // for ConstRef, Ref, ConstStringRef, ConstStringListRef, StringRef

namespace ftxui {
struct ButtonOption;
struct CheckboxOption;
struct Event;
struct InputOption;
struct MenuOption;
struct RadioboxOption;
struct MenuEntryOption;

template <class T, class... Args>
std::shared_ptr<T> Make(Args&&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

// Pipe operator to decorate components.
using ComponentDecorator = std::function<component_ptr_t(component_ptr_t)>;
using ElementDecorator = std::function<Element(Element)>;
component_ptr_t operator|(component_ptr_t component, ComponentDecorator decorator);
component_ptr_t operator|(component_ptr_t component, ElementDecorator decorator);
component_ptr_t& operator|=(component_ptr_t& component, ComponentDecorator decorator);
component_ptr_t& operator|=(component_ptr_t& component, ElementDecorator decorator);

namespace Container {
component_ptr_t Vertical(Components children);
component_ptr_t Vertical(Components children, int* selector);
component_ptr_t Horizontal(Components children);
component_ptr_t Horizontal(Components children, int* selector);
component_ptr_t Tab(Components children, int* selector);
component_ptr_t Stacked(Components children);
}  // namespace Container

component_ptr_t Button(ButtonOption options);
component_ptr_t Button(ConstStringRef label,
                 std::function<void()> on_click,
                 ButtonOption options = ButtonOption::Simple());

component_ptr_t Checkbox(CheckboxOption options);
component_ptr_t Checkbox(ConstStringRef label,
                   bool* checked,
                   CheckboxOption options = CheckboxOption::Simple());

component_ptr_t Input(InputOption options = {});
component_ptr_t Input(StringRef content, InputOption options = {});
component_ptr_t Input(StringRef content,
                StringRef placeholder,
                InputOption options = {});

component_ptr_t Menu(MenuOption options);
component_ptr_t Menu(ConstStringListRef entries,
               int* selected_,
               MenuOption options = MenuOption::Vertical());
component_ptr_t MenuEntry(MenuEntryOption options);
component_ptr_t MenuEntry(ConstStringRef label, MenuEntryOption options = {});

component_ptr_t Radiobox(RadioboxOption options);
component_ptr_t Radiobox(ConstStringListRef entries,
                   int* selected_,
                   RadioboxOption options = {});

component_ptr_t Dropdown(ConstStringListRef entries, int* selected);
component_ptr_t Toggle(ConstStringListRef entries, int* selected);

// General slider constructor:
template <typename T>
component_ptr_t Slider(SliderOption<T> options);

// Shorthand without the `SliderOption` constructor:
component_ptr_t Slider(ConstStringRef label,
                 Ref<int> value,
                 ConstRef<int> min = 0,
                 ConstRef<int> max = 100,
                 ConstRef<int> increment = 5);
component_ptr_t Slider(ConstStringRef label,
                 Ref<float> value,
                 ConstRef<float> min = 0.f,
                 ConstRef<float> max = 100.f,
                 ConstRef<float> increment = 5.f);
component_ptr_t Slider(ConstStringRef label,
                 Ref<long> value,
                 ConstRef<long> min = 0l,
                 ConstRef<long> max = 100l,
                 ConstRef<long> increment = 5l);

component_ptr_t ResizableSplit(ResizableSplitOption options);
component_ptr_t ResizableSplitLeft(component_ptr_t main, component_ptr_t back, int* main_size);
component_ptr_t ResizableSplitRight(component_ptr_t main, component_ptr_t back, int* main_size);
component_ptr_t ResizableSplitTop(component_ptr_t main, component_ptr_t back, int* main_size);
component_ptr_t ResizableSplitBottom(component_ptr_t main, component_ptr_t back, int* main_size);

component_ptr_t Renderer(component_ptr_t child, std::function<Element()>);
component_ptr_t Renderer(std::function<Element()>);
component_ptr_t Renderer(std::function<Element(bool /* focused */)>);
ComponentDecorator Renderer(ElementDecorator);

component_ptr_t CatchEvent(component_ptr_t child, std::function<bool(Event)>);
ComponentDecorator CatchEvent(std::function<bool(Event)> on_event);

component_ptr_t Maybe(component_ptr_t, const bool* show);
component_ptr_t Maybe(component_ptr_t, std::function<bool()>);
ComponentDecorator Maybe(const bool* show);
ComponentDecorator Maybe(std::function<bool()>);

component_ptr_t Modal(component_ptr_t main, component_ptr_t modal, const bool* show_modal);
ComponentDecorator Modal(component_ptr_t modal, const bool* show_modal);

component_ptr_t Collapsible(ConstStringRef label,
                      component_ptr_t child,
                      Ref<bool> show = false);

component_ptr_t Hoverable(component_ptr_t component, bool* hover);
component_ptr_t Hoverable(component_ptr_t component,
                    std::function<void()> on_enter,
                    std::function<void()> on_leave);
component_ptr_t Hoverable(component_ptr_t component,  //
                    std::function<void(bool)> on_change);
ComponentDecorator Hoverable(bool* hover);
ComponentDecorator Hoverable(std::function<void()> on_enter,
                             std::function<void()> on_leave);
ComponentDecorator Hoverable(std::function<void(bool)> on_change);

component_ptr_t Window(WindowOptions option);

}  // namespace ftxui

#endif /* end of include guard: FTXUI_COMPONENT_HPP */
