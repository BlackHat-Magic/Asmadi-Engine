#include <ecs/ecs.h>
#include <material/m_common.h>

UIComponent create_ui_component (AppState* state, Uint32 max_rects);

void draw_rectangle (UIComponent* ui, float x, float y, float w, float h, float r, float g, float b, float a);