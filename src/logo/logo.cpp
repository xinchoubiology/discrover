#include <iostream>
#include <cmath>
#include <vector>
#include <cairo.h>
#include <cairo-pdf.h>
#include "logo.hpp"

namespace logo {
using namespace std;

const double factor = 100.0;
const double node_width = 0.75 * factor;
const double node_height = 1.0 * factor;

struct coord_t {
  double x;
  double y;
};

struct rgb_t {
  double r;
  double g;
  double b;
};

struct palette_t {
  rgb_t a;
  rgb_t c;
  rgb_t g;
  rgb_t t;
};

palette_t default_colors = {
  {0.0, 0.75, 0.0},
  {0.0, 0.0, 1.0},
  {1, 0.6470588, 0.0},
  {1.0, 0.0, 0.0}
};

using coords_t = vector<coord_t>;

coords_t letter_a = {
  {0, 0},
  {4, 10},
  {6, 10},
  {10, 0},
  {8, 0},
  {6.8, 3},
  {3.2, 3},
  {2, 0},
  {0, 0},
  {3.6, 4},
  {5, 7.5},
  {6.4, 4},
  {3.6, 4}
};

coords_t letter_t = {
  {10, 10},
  {10, 9},
  {6, 9},
  {6, 0},
  {4, 0},
  {4, 9},
  {0, 9},
  {0, 10}
};

void draw_letter(cairo_t *cr, const coords_t &coords, const coord_t &origin, double width, double height, rgb_t color) {
  cairo_move_to(cr, origin.x + coords[0].x / 10 * width, origin.y - coords[0].y / 10 * height);
  for(auto coord: coords)
    cairo_line_to(cr, origin.x + coord.x / 10 * width, origin.y - coord.y / 10 * height);
  cairo_set_line_width (cr, 1.0);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
  cairo_set_source_rgb (cr, color.r, color.g, color.b);
  cairo_fill(cr);
}

void draw_letter_a(cairo_t *cr, const coord_t &coord, double width, double height, rgb_t color) {
  draw_letter(cr, letter_a, coord, width, height, color);
}

void draw_letter_c(cairo_t *cr, const coord_t &coord, double width, double height, rgb_t color) {
  if(height > 0) {
    cairo_save(cr);
    cairo_rectangle(cr, coord.x, coord.y - height, 0.5 * width, height);
    cairo_rectangle(cr, coord.x, coord.y - 0.35 * height, width, 0.35 * height);
    cairo_rectangle(cr, coord.x, coord.y - height, width, 0.35 * height);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
    cairo_save(cr);
    cairo_translate(cr, coord.x + 0.5 * width, coord.y - 0.5 * height);
    cairo_scale(cr, 1.0 * width / height, 1.0);
    cairo_arc(cr, 0, 0, 0.5 * height, 0, M_PI * 2);
    cairo_arc(cr, 0, 0, 0.5 * 0.8 * height, 0, M_PI * 2);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_set_source_rgb (cr, color.r, color.g, color.b);
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_restore(cr);
  }
}

void draw_letter_g(cairo_t *cr, const coord_t &coord, double width, double height, rgb_t color) {
  draw_letter_c(cr, coord, width, height, color);
  cairo_rectangle(cr, coord.x + 0.5 * width, coord.y - 0.45 * height, 0.5 * width, 0.1 * height);
  cairo_rectangle(cr, coord.x + 0.9 * width, coord.y - 0.45 * height, 0.1 * width, 0.45 * height);
  cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
  cairo_set_source_rgb (cr, color.r, color.g, color.b);
  cairo_fill(cr);
}

void draw_letter_t(cairo_t *cr, const coord_t &coord, double width, double height, rgb_t color) {
  draw_letter(cr, letter_t, coord, width, height, color);
}

double information_content(const column_t &col) {
  double x = 0;
  for(auto &y: col)
    if(y > 0)
      x -= y * log(y) / log(2.0);
  return(2 - x);
}

void draw_logo_to_surface(cairo_surface_t *surface, const matrix_t &matrix) {
  cairo_t *cr = cairo_create (surface);

  coord_t current = {0, node_height};
  for(auto &col: matrix) {
    double col_height = information_content(col) / 2;
    current.y = node_height;

    double current_height = col[0] * node_height * col_height;
    draw_letter_a(cr, current, node_width, current_height, default_colors.a);
    current.y -= current_height;

    current_height = col[1] * node_height * col_height;
    draw_letter_c(cr, current, node_width, current_height, default_colors.c);
    current.y -= current_height;

    current_height = col[2] * node_height * col_height;
    draw_letter_g(cr, current, node_width, current_height, default_colors.g);
    current.y -= current_height;

    current_height = col[3] * node_height * col_height;
    draw_letter_t(cr, current, node_width, current_height, default_colors.t);

    current.x += node_width;
  }

  cairo_show_page(cr);
  cairo_destroy(cr);
}

string ending(output_t kind) {
  switch(kind) {
    case output_t::PDF:
      return "pdf";
    case output_t::PNG:
      return "png";
    default:
      return "";
  };
}

bool draw_logo(const matrix_t &matrix, const string &path, output_t kind) {
  string out_path = path + "." + ending(kind);
  cout << "Generating logo in " << out_path << "." << endl;

  double width = node_width * matrix.size();
  double height = node_height;

  cairo_surface_t *surface;
  switch(kind) {
    case output_t::PDF:
      surface = cairo_pdf_surface_create (out_path.c_str(), width, height);
      break;
    case output_t::PNG:
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
      break;
    default:
      return EXIT_FAILURE;
  }

  draw_logo_to_surface(surface, matrix);

  switch(kind) {
    case output_t::PDF:
      cairo_surface_flush(surface);
      break;
    case output_t::PNG:
      cairo_surface_write_to_png(surface, out_path.c_str());
      break;
  }
  cairo_surface_destroy (surface);
  return EXIT_SUCCESS;
}
};