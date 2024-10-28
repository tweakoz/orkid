// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_DOM_ELEMENTS_HPP
#define FTXUI_DOM_ELEMENTS_HPP

#include <functional>
#include <memory>

#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/direction.hpp>
#include <ftxui/dom/flexbox_config.hpp>
#include <ftxui/dom/linear_gradient.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/terminal.hpp>
#include <ftxui/util/ref.hpp>

namespace ftxui {
class Node;
using Element = std::shared_ptr<Node>;
using Elements = std::vector<Element>;
using node_ptr_t = std::shared_ptr<Node>;
using node_vect_t = std::vector<node_ptr_t>;

using Decorator = std::function<node_ptr_t(node_ptr_t)>;
using GraphFunction = std::function<std::vector<int>(int, int)>;

enum BorderStyle {
  LIGHT,
  DASHED,
  HEAVY,
  DOUBLE,
  ROUNDED,
  EMPTY,
};

// Pipe node_vect_t into decorator togethers.
// For instance the next lines are equivalents:
// -> text("ftxui") | bold | underlined
// -> underlined(bold(text("FTXUI")))
node_ptr_t operator|(node_ptr_t, Decorator);
node_ptr_t& operator|=(Element&, Decorator);
node_vect_t operator|(Elements, Decorator);
Decorator operator|(Decorator, Decorator);

// --- Widget ---
node_ptr_t text(std::string text);
node_ptr_t vtext(std::string text);
node_ptr_t separator();
node_ptr_t separatorLight();
node_ptr_t separatorDashed();
node_ptr_t separatorHeavy();
node_ptr_t separatorDouble();
node_ptr_t separatorEmpty();
node_ptr_t separatorStyled(BorderStyle);
node_ptr_t separator(Pixel);
node_ptr_t separatorCharacter(std::string);
node_ptr_t separatorHSelector(float left,
                           float right,
                           Color unselected_color,
                           Color selected_color);
node_ptr_t separatorVSelector(float up,
                           float down,
                           Color unselected_color,
                           Color selected_color);
node_ptr_t gauge(float progress);
node_ptr_t gaugeLeft(float progress);
node_ptr_t gaugeRight(float progress);
node_ptr_t gaugeUp(float progress);
node_ptr_t gaugeDown(float progress);
node_ptr_t gaugeDirection(float progress, Direction direction);
node_ptr_t border(node_ptr_t);
node_ptr_t borderLight(node_ptr_t);
node_ptr_t borderDashed(node_ptr_t);
node_ptr_t borderHeavy(node_ptr_t);
node_ptr_t borderDouble(node_ptr_t);
node_ptr_t borderRounded(node_ptr_t);
node_ptr_t borderEmpty(node_ptr_t);
Decorator borderStyled(BorderStyle);
Decorator borderStyled(BorderStyle, Color);
Decorator borderStyled(Color);
Decorator borderWith(const Pixel&);
node_ptr_t window(node_ptr_t title, node_ptr_t content);
node_ptr_t spinner(int charset_index, size_t image_index);
node_ptr_t paragraph(const std::string& text);
node_ptr_t paragraphAlignLeft(const std::string& text);
node_ptr_t paragraphAlignRight(const std::string& text);
node_ptr_t paragraphAlignCenter(const std::string& text);
node_ptr_t paragraphAlignJustify(const std::string& text);
node_ptr_t graph(GraphFunction);
node_ptr_t emptyElement();
node_ptr_t canvas(ConstRef<Canvas>);
node_ptr_t canvas(int width, int height, std::function<void(Canvas&)>);
node_ptr_t canvas(std::function<void(Canvas&)>);

// -- Decorator ---
node_ptr_t bold(node_ptr_t);
node_ptr_t dim(node_ptr_t);
node_ptr_t inverted(node_ptr_t);
node_ptr_t underlined(node_ptr_t);
node_ptr_t underlinedDouble(node_ptr_t);
node_ptr_t blink(node_ptr_t);
node_ptr_t strikethrough(node_ptr_t);
Decorator color(Color);
Decorator bgcolor(Color);
Decorator color(const LinearGradient&);
Decorator bgcolor(const LinearGradient&);
node_ptr_t color(Color, Element);
node_ptr_t bgcolor(Color, Element);
node_ptr_t color(const LinearGradient&, Element);
node_ptr_t bgcolor(const LinearGradient&, Element);
Decorator focusPosition(int x, int y);
Decorator focusPositionRelative(float x, float y);
node_ptr_t automerge(node_ptr_t child);
Decorator hyperlink(std::string link);
node_ptr_t hyperlink(std::string link, node_ptr_t child);

// --- Layout is
// Horizontal, Vertical or stacked set of elements.
node_ptr_t hbox(Elements);
node_ptr_t vbox(Elements);
node_ptr_t dbox(Elements);
node_ptr_t flexbox(Elements, FlexboxConfig config = FlexboxConfig());
node_ptr_t gridbox(std::vector<Elements> lines);

node_ptr_t hflow(Elements);  // Helper: default flexbox with row direction.
node_ptr_t vflow(Elements);  // Helper: default flexbox with column direction.

// -- Flexibility ---
// Define how to share the remaining space when not all of it is used inside a
// container.
node_ptr_t flex(node_ptr_t);         // Expand/Minimize if possible/needed.
node_ptr_t flex_grow(node_ptr_t);    // Expand node_ptr_t if possible.
node_ptr_t flex_shrink(node_ptr_t);  // Minimize node_ptr_t if needed.

node_ptr_t xflex(node_ptr_t);         // Expand/Minimize if possible/needed on X axis.
node_ptr_t xflex_grow(node_ptr_t);    // Expand node_ptr_t if possible on X axis.
node_ptr_t xflex_shrink(node_ptr_t);  // Minimize node_ptr_t if needed on X axis.

node_ptr_t yflex(node_ptr_t);         // Expand/Minimize if possible/needed on Y axis.
node_ptr_t yflex_grow(node_ptr_t);    // Expand node_ptr_t if possible on Y axis.
node_ptr_t yflex_shrink(node_ptr_t);  // Minimize node_ptr_t if needed on Y axis.

node_ptr_t notflex(node_ptr_t);  // Reset the flex attribute.
node_ptr_t filler();          // A blank expandable element.

// -- Size override;
enum WidthOrHeight { WIDTH, HEIGHT };
enum Constraint { LESS_THAN, EQUAL, GREATER_THAN };
Decorator size(WidthOrHeight, Constraint, int value);

// --- Frame ---
// A frame is a scrollable area. The internal area is potentially larger than
// the external one. The internal area is scrolled in order to make visible the
// focused element.
node_ptr_t frame(node_ptr_t);
node_ptr_t xframe(node_ptr_t);
node_ptr_t yframe(node_ptr_t);
node_ptr_t focus(node_ptr_t);
node_ptr_t select(node_ptr_t);

// --- Cursor ---
// Those are similar to `focus`, but also change the shape of the cursor.
node_ptr_t focusCursorBlock(node_ptr_t);
node_ptr_t focusCursorBlockBlinking(node_ptr_t);
node_ptr_t focusCursorBar(node_ptr_t);
node_ptr_t focusCursorBarBlinking(node_ptr_t);
node_ptr_t focusCursorUnderline(node_ptr_t);
node_ptr_t focusCursorUnderlineBlinking(node_ptr_t);

// --- Misc ---
node_ptr_t vscroll_indicator(node_ptr_t);
Decorator reflect(Box& box);
// Before drawing the |element| clear the pixel below. This is useful in
// combinaison with dbox.
node_ptr_t clear_under(node_ptr_t element);

// --- Util --------------------------------------------------------------------
node_ptr_t hcenter(node_ptr_t);
node_ptr_t vcenter(node_ptr_t);
node_ptr_t center(node_ptr_t);
node_ptr_t align_right(node_ptr_t);
node_ptr_t nothing(node_ptr_t element);

namespace Dimension {
Dimensions Fit(Element&);
}  // namespace Dimension

}  // namespace ftxui

// Make container able to take any number of children as input.
#include <ftxui/dom/take_any_args.hpp>

// Include old definitions using wstring.
#include <ftxui/dom/deprecated.hpp>
#endif  // FTXUI_DOM_ELEMENTS_HPP
