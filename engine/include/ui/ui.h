#include <ecs/ecs.h>
#include <material/m_common.h>

UIComponent create_ui_component (const AppState* state, const Uint32 max_rects, const char* font_path, const float ptsize);

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