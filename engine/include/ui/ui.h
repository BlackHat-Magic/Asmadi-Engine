#include <ecs/ecs.h>
#include <material/m_common.h>

UIComponent create_ui_component (const AppState* state, const Uint32 max_rects, const Uint32 max_texts, const char* font_path, const float ptsize);

void draw_rectangle (
    UIComponent* ui,
    const float x,
    const float y,
    const float w,
    const float h,
    const float r,
    const float g,
    const float b,
    const float a
);

int draw_text(UIComponent* ui, const AppState* state, const char* utf8,
    float x, float y, float r, float g, float b, float a);

float measure_text_width(const UIComponent* ui, const char* utf8);

void measure_text(const UIComponent* ui, const char* utf8, int* w, int* h);