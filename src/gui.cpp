#include "gui.h"
#include "d3d11_util.h"
#include <imgui/imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>
#include "../data/Font_Roboto.h"
#include "Craft_Jobs.h"
#include "Craft_Actions.h"
#include "types.h"
#include "global_data.h"
#include "crafting_solver3.h"
#include "time.h"
#include "../generated_src/compressed_data.h"

#define MAX_NUM_SOLVER_THREADS 64U

Global_Data global_data;

void copy_actions_to_clipboard_teamcraft(Craft_Action *actions, s32 num_actions) {
    char buf[4096] = {};
    char *ptr = buf;

    ptr += snprintf(ptr, 4096 - (ptr - buf), "[");
    for (int i = 0; i < num_actions && ptr - buf < 4096; i++)
        ptr += snprintf(ptr, 4096 - (ptr - buf), "%s\"%s\"", i == 0 ? "" : ", ", Craft_Action_to_Teamcraft_string[actions[i]].data);
    ptr += snprintf(ptr, 4096 - (ptr - buf), "]");

    ImGui::SetClipboardText(buf);
}

void copy_actions_to_clipboard_macro(Craft_Action *actions, s32 num_actions) {
    char buf[4096] = {};
    char *ptr = buf;

    ptr += snprintf(ptr, 4096 - (ptr - buf), "/mlock\r\n");
    for (int i = 0; i < num_actions && ptr - buf < 4096; i++)
        ptr += snprintf(ptr, 4096 - (ptr - buf), "/ac \"%s\" <wait.3>\r\n", Craft_Action_to_string[actions[i]].data);
    ptr += snprintf(ptr, 4096 - (ptr - buf), "/echo Craft finished <se.1>");

    ImGui::SetClipboardText(buf);
}

float CalcButtonWidth(const char *text) {
    auto &style = ImGui::GetStyle();
    float text_width = ImGui::CalcTextSize(text, 0, true).x;
    return text_width + style.FramePadding.x * 2.0f;
}

void SetNextItemMaxWidth(float item_width) {
    auto window = ImGui::GetCurrentWindow();
    float width_left_in_work_rect = window->WorkRect.GetWidth() - window->DC.CursorPos.x;
    item_width = clamp(item_width, 0.0f, width_left_in_work_rect);
    ImGui::SetNextItemWidth(item_width);
}

void PreferredSameLine(float width) {
    auto available_width = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    if (ImGui::GetItemRectMax().x + width < available_width)
        ImGui::SameLine();
}

bool AlignedMenuItem(const char *label, bool selected) {
    float w = ImGui::CalcTextSize(label, 0, true).x;
    return ImGui::Selectable(label, selected, 0, ImVec2(w, 0.0f));
}

struct Drag_Drop_Payload_Craft_Action {
    Craft_Job job;
    Craft_Action action;
    u32 index_plus_one;
};

struct Registered_Button {
    ImGuiID id;
    ImRect bb;
    ImDrawList *draw_list;
    bool pressed;
    bool hovered;
    bool held;
};

bool FFUI_Checkbox(const char *label, bool *v) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiStyle &style = GImGui->Style;
    ImGuiID id = window->GetID(label);
    ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    float square_sz = ImGui::GetFrameHeight();
    ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));
    
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
        *v = !(*v);

    ImGui::RenderNavHighlight(total_bb, id);

    ImDrawList *draw_list = window->DrawList;
    ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));

    f32x4 checked_uvs   = FFUI_uvs[FFUI_Checkbox_checked];
    f32x4 unchecked_uvs = FFUI_uvs[FFUI_Checkbox_unchecked];

    draw_list->AddImage(global_data.actions_texture, check_bb.Min, check_bb.Max, unchecked_uvs.xy, unchecked_uvs.zw, 0xffffffff);
    if (*v)
        draw_list->AddImage(global_data.actions_texture, check_bb.Min, check_bb.Max, checked_uvs.xy, checked_uvs.zw, 0xffffffff);

    ImRect hover_bb = check_bb;
    hover_bb.Expand(square_sz * -0.22f);

    if(hovered)
        draw_list->AddRectFilled(hover_bb.Min, hover_bb.Max, 0x3fffffff);

    ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y);
    if (label_size.x > 0.0f)
        ImGui::RenderText(label_pos, label);

    return pressed;
}

bool FFUI_Button(const char *label, bool same_line_if_enough_space = true, bool extend_to_label_width = false) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiStyle &style = GImGui->Style;
    ImGuiID id = window->GetID(label);

    ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    f32 height = label_size.y * 1.75f;

    ImVec2 left_size = ImVec2(height * 24.0f / 52.0f, height);
    ImVec2 right_size = ImVec2(height * 24.0f / 52.0f, height);
    ImVec2 middle_size = ImVec2(ImGui::GetFontSize() * 6.0f, height);

    if (extend_to_label_width)
        middle_size.x = label_size.x;

    if (same_line_if_enough_space)
        PreferredSameLine(left_size.x + middle_size.x + right_size.x);

    ImVec2 pos = window->DC.CursorPos;

    ImVec2 left_start = pos;
    ImVec2 middle_start = ImVec2(pos.x + left_size.x, pos.y);
    ImVec2 right_start = ImVec2(pos.x + left_size.x + middle_size.x, pos.y);

    ImRect left_bb = ImRect(left_start, left_start + left_size);
    ImRect right_bb = ImRect(right_start, right_start + right_size);
    ImRect middle_bb = ImRect(middle_start, middle_start + middle_size);

    f32 width = left_size.x + middle_size.x + right_size.x;

    ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));

    //if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
    //    pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    //ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImGui::RenderNavHighlight(bb, id);

    ImDrawList *draw_list = window->DrawList;

    f32x4 left_uvs = FFUI_uvs[FFUI_Button_left];
    f32x4 middle_uvs = FFUI_uvs[FFUI_Button_middle];
    f32x4 right_uvs = FFUI_uvs[FFUI_Button_right];

    draw_list->AddImage(global_data.actions_texture, left_bb.Min, left_bb.Max, left_uvs.xy, left_uvs.zw, 0xffffffff);
    draw_list->AddImage(global_data.actions_texture, middle_bb.Min, middle_bb.Max, middle_uvs.xy, middle_uvs.zw, 0xffffffff);
    draw_list->AddImage(global_data.actions_texture, right_bb.Min, right_bb.Max, right_uvs.xy, right_uvs.zw, 0xffffffff);

    if (hovered)
        draw_list->AddRectFilled(bb.Min + ImVec2(0, height * 0.1f), bb.Max - ImVec2(0, height * 0.18f), 0x1fffffff, height / 2.0f, ImDrawFlags_None);

    ImGui::RenderTextClipped(middle_bb.Min, middle_bb.Max, label, NULL, &label_size, style.ButtonTextAlign, &middle_bb);

    return pressed;
}

Registered_Button FFUI_register_ActionButton(ImGuiID id, bool same_line_if_enough_space = true) {
    Registered_Button result = {};
    result.id = id;

    if (same_line_if_enough_space)
        PreferredSameLine(global_data.button_size);

    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return result;

    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, ImVec2(pos.x + global_data.button_size, pos.y + global_data.button_size));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return result;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImGui::RenderNavHighlight(bb, id);

    result.bb = bb;
    result.draw_list = window->DrawList;
    result.pressed = pressed;
    result.hovered = hovered;
    result.held = held;

    return result;
}

void FFUI_draw_ActionButton_empty(Registered_Button button) {
    if (!button.draw_list)
        return;

    f32x4 uv = FFUI_uvs[FFUI_ActionButton_Empty];
    button.draw_list->AddImage(global_data.actions_texture, button.bb.Min, button.bb.Max, uv.xy, uv.zw, 0xffffffff);
}

void FFUI_draw_ActionButton_internal(Registered_Button button, ImTextureID texture, f32x4 uv, bool enabled = true, bool selected = false) {
    if (!button.draw_list)
        return;

    ImVec2 image_size = (button.bb.Max - button.bb.Min) * -(1.0f - (80.0f / 88.0f));
    ImVec2 selection_size = (button.bb.Max - button.bb.Min) * -(1.0f - (112.0f / 88.0f));

    ImRect image_bb = ImRect(button.bb.Min - image_size / 2.0f, button.bb.Max + image_size / 2.0f);
    ImRect slot_bb = button.bb;
    ImRect selection_bb = ImRect(button.bb.Min - selection_size / 2.0f, button.bb.Max + selection_size / 2.0f);

    button.draw_list->AddImage(texture, image_bb.Min, image_bb.Max, uv.xy, uv.zw, enabled ? 0xffffffff : 0xff3f3f3f);

    if (button.hovered)
        button.draw_list->AddRectFilled(image_bb.Min, image_bb.Max, 0x3fffffff);

    f32x4 slot_uvs = FFUI_uvs[FFUI_ActionButton_Filled];
    button.draw_list->AddImage(global_data.actions_texture, slot_bb.Min, slot_bb.Max, slot_uvs.xy, slot_uvs.zw, 0xffffffff);
}

void FFUI_draw_ActionButton_job(Registered_Button button, Craft_Job job, bool selected = false) {
    if (!button.draw_list)
        return;

    f32x4 uv = Craft_Job_Icon_uvs[job];
    FFUI_draw_ActionButton_internal(button, global_data.actions_texture, uv, true, selected);
}

void FFUI_draw_ActionButton_action(Registered_Button button, Craft_Job job, Craft_Action action, Craft_Action previous_action = (Craft_Action)0xFF, bool enabled = true, bool selected = false) {
    if (!button.draw_list)
        return;

    f32x4 uv = Craft_Action_Icon_uvs[job][action];
    FFUI_draw_ActionButton_internal(button, global_data.actions_texture, uv, enabled, selected);

    int32_t cp_cost = Craft_Actions_efficiency[action].x;
    if (cp_cost) {
        if (action == CA_Standard_Touch && previous_action == CA_Basic_Touch)
            cp_cost = 18;

        float font_size = ImGui::GetFontSize();
        ImVec2 cp_pos = ImVec2(button.bb.Min.x + font_size / 4.0f, button.bb.Max.y - font_size / 2.0f);

        char cp_buf[16];
        snprintf(cp_buf, sizeof(cp_buf), "%d", cp_cost);
        button.draw_list->AddText(cp_pos, 0xffdf50df, cp_buf);
    }
}

bool FFUI_ActionButton_job(ImGuiID id, Craft_Job job, bool selected = false, bool same_line_if_enough_space = true) {
    auto button = FFUI_register_ActionButton(id, same_line_if_enough_space);
    FFUI_draw_ActionButton_job(button, job, selected);
    return button.pressed;
}

bool FFUI_ActionButton_action(ImGuiID id, Craft_Job job, Craft_Action action, Craft_Action previous_action = (Craft_Action)0xFF, bool enabled = true, bool selected = false, bool drag_drop_source = false, bool same_line_if_enough_space = true) {
    auto button = FFUI_register_ActionButton(id, same_line_if_enough_space);
    FFUI_draw_ActionButton_action(button, job, action, previous_action, enabled, selected);

    if (drag_drop_source) {
        if (ImGui::BeginDragDropSource()) {
            FFUI_ActionButton_action(id, job, action, previous_action);

            Drag_Drop_Payload_Craft_Action payload = {};
            payload.job = job;
            payload.action = action;

            ImGui::SetDragDropPayload("Craft_Action", &payload, sizeof(payload));
            ImGui::EndDragDropSource();
        }
    }

    if (button.hovered) {
        ImGui::BeginTooltip();

        ImGui::Text("%s", Craft_Action_to_string[action].data);
        ImGui::Separator();

        auto efficiency = Craft_Actions_efficiency[action];

        if (efficiency.x) {
            ImGui::TextColored({ 0.882f, 0.314f, 0.882f, 1.0f }, "CP:"); ImGui::SameLine();
            ImGui::Text("%d", (int)efficiency.x);
        }
        
        if (ImMax(efficiency.z, efficiency.w)) {
            ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Efficiency:"); ImGui::SameLine();
            ImGui::Text("%d%%", (int)(ImMax(efficiency.z, efficiency.w) * 100.0f));
        }

        if (efficiency.y) {
            ImGui::TextColored({ 0.6f, 0.6f, 0.6f, 1.0f }, "Durability:"); ImGui::SameLine();
            ImGui::Text("%d", (int)efficiency.y);
        }

        ImGui::Separator();

        ImGui::Text("Level: %d", Craft_Actions_min_lvl[action]);

        ImGui::EndTooltip();
    }

    return button.pressed;
}

void job_selector(Craft_Job &selected_job) {
    if (selected_job == (Craft_Job)-1) {
        for (int job = 0; job < NUM_JOBS; job++)
            if (FFUI_ActionButton_job(ImGui::GetID(job), (Craft_Job)job))
                selected_job = (Craft_Job)job;
        return;
    }

    if (FFUI_ActionButton_job(ImGui::GetID("selected_job"), selected_job)) {
        selected_job = (Craft_Job)-1;
        return;
    }
    ImGui::SameLine();

    ImGui::PushFont(global_data.big_font);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", Craft_Job_to_string[selected_job].data);
    ImGui::PopFont();
}

void craft_actions_selector(Craft_Job job, u32 &active_actions, u32 visible_actions, Craft_Action previous_action = (Craft_Action)-1, bool drag_drop_source = false) {
    if (!(visible_actions & ALL_ACTIONS)) {
        ImGui::TextUnformatted("No actions selected in Profile.");
        return;
    }

    float available_width = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    float per_action_width = ImGui::GetStyle().ItemSpacing.x + global_data.button_size;

    if (visible_actions & ALL_ACTIONS_PROGRESS) {
        ImGui::BeginGroup();
        ImGui::Text("Progress");
        ImGui::PushID(Craft_Actions_Progress);
        bool has_displayed_first = false;
        for (int i = 0; i < NUM_ACTIONS_PROGRESS; i++) {
            auto action = Craft_Actions_Progress[i];

            if (visible_actions & (1 << action)) {
                if (FFUI_ActionButton_action(ImGui::GetID(action), job, action, previous_action, active_actions & (1 << action), false, drag_drop_source, has_displayed_first))
                    active_actions ^= 1 << action;

                has_displayed_first = true;
            }
        }

        ImGui::PopID();
        ImGui::EndGroup();

        if (ImGui::GetItemRectMax().x + per_action_width * IM_ARRAYSIZE(Craft_Actions_Quality) + 2 * ImGui::GetStyle().ItemSpacing.x < available_width && visible_actions & ALL_ACTIONS_QUALITY)
            ImGui::SameLine(0.0f, 3 * ImGui::GetStyle().ItemSpacing.x);
    }

    if (visible_actions & ALL_ACTIONS_QUALITY) {
        ImGui::BeginGroup();
        ImGui::Text("Quality");
        ImGui::PushID(Craft_Actions_Quality);
        bool has_displayed_first = false;
        for (int i = 0; i < NUM_ACTIONS_QUALITY; i++) {
            auto action = Craft_Actions_Quality[i];

            if (visible_actions & (1 << action)) {
                if (FFUI_ActionButton_action(ImGui::GetID(action), job, action, previous_action, active_actions & (1 << action), false, drag_drop_source, has_displayed_first))
                    active_actions ^= 1 << action;

                has_displayed_first = true;
            }
        }

        ImGui::PopID();
        ImGui::EndGroup();
    }

    if (visible_actions & ALL_ACTIONS_BUFF) {
        ImGui::BeginGroup();
        ImGui::Text("Buff");
        ImGui::PushID(Craft_Actions_Buff);
        bool has_displayed_first = false;
        for (int i = 0; i < NUM_ACTIONS_BUFF; i++) {
            auto action = Craft_Actions_Buff[i];

            if (visible_actions & (1 << action)) {
                if (FFUI_ActionButton_action(ImGui::GetID(action), job, action, previous_action, active_actions & (1 << action), false, drag_drop_source, has_displayed_first))
                    active_actions ^= 1 << action;

                has_displayed_first = true;
            }
        }

        ImGui::PopID();
        ImGui::EndGroup();

        if (ImGui::GetItemRectMax().x + per_action_width * IM_ARRAYSIZE(Craft_Actions_Other) + 2 * ImGui::GetStyle().ItemSpacing.x < available_width && visible_actions & ALL_ACTIONS_OTHER)
            ImGui::SameLine(0.0f, 3 * ImGui::GetStyle().ItemSpacing.x);
    }


    if (visible_actions & ALL_ACTIONS_OTHER) {
        ImGui::BeginGroup();
        ImGui::Text("Other");
        ImGui::PushID(Craft_Actions_Other);
        bool has_displayed_first = false;
        for (int i = 0; i < NUM_ACTIONS_OTHER; i++) {
            auto action = Craft_Actions_Other[i];

            if (visible_actions & (1 << action)) {
                if (FFUI_ActionButton_action(ImGui::GetID(action), job, action, previous_action, active_actions & (1 << action), false, drag_drop_source, has_displayed_first))
                    active_actions ^= 1 << action;

                has_displayed_first = true;
            }
        }

        ImGui::PopID();
        ImGui::EndGroup();
    }
}

bool recipe_selector(Craft_Job job, Recipe *&selected_recipe) {
    auto input_str_width = global_data.input_str_width;
    bool has_selected_new_recipe = false;

    if (!selected_recipe) {
        static char recipe_search[64] = "";
        SetNextItemMaxWidth(input_str_width);
        ImGui::InputTextWithHint("##recipe_search", "Recipe search", recipe_search, sizeof(recipe_search));

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.254f, 0.250f, 0.254f, 1.000f));
        SetNextItemMaxWidth(input_str_width);
        if (ImGui::BeginListBox("##recipe_selector", ImVec2(0, -1))) {
            for (int i = 0; i < NUM_RECIPES[job]; i++) {
                auto &recipe = Recipes[job][i];
                auto &result = recipe.result;
                auto &result_name = Item_to_name[result];
                if (contains_ignoring_case(result_name, { (int64_t)strlen(recipe_search), recipe_search }))
                    if (ImGui::Selectable(result_name.data, false)) {
                        selected_recipe = (Recipe *)&recipe;
                        has_selected_new_recipe = true;
                    }
            }
            ImGui::EndListBox();
        }
        ImGui::PopStyleColor();

        return has_selected_new_recipe;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Recipe: ");
    if (FFUI_Button(Item_to_name[selected_recipe->result].data, true, true))
        selected_recipe = 0;

    return false;
}

void FFUI_ProgressBar(s32 current_value, s32 max_value, u32 left_color = 0xFF4AA24A, u32 right_color = 0xFF39D3B5, u32 full_separators = 0, u32 half_separators = 0) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    const ImGuiStyle &style = GImGui->Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(ImVec2(0, 0), ImGui::CalcItemWidth(), GImGui->FontSize + style.FramePadding.y * 2.0f);
    ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, 0))
        return;

    ImDrawList *draw_list = window->DrawList;

    float fraction = ImSaturate(current_value / (float)max_value);
    ImRect filled_bb = ImRect(bb.Min, ImVec2(ImLerp(bb.Min.x, bb.Max.x, fraction), bb.Max.y));

    draw_list->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), 0.0f);
    draw_list->AddRectFilledMultiColor(filled_bb.Min, filled_bb.Max, left_color, right_color, right_color, left_color);

    u32 separator_color = ImGui::GetColorU32(ImGuiCol_WindowBg);

    if (full_separators) {
        s32 num = (max_value - 1) / full_separators;

        for (int i = 0; i < num; i++) {
            s32 val = (i + 1) * full_separators;
            float x = ImLerp(bb.Min.x, bb.Max.x, val / (float)max_value);
            draw_list->AddLine(ImVec2(x, bb.Min.y), ImVec2(x, bb.Max.y), separator_color);
        }
    }

    if (half_separators) {
        ImVec2 y = ImVec2();
        s32 num = (max_value - 1) / half_separators;

        for (int i = 0; i < num; i++) {
            if (full_separators && ((i + 1) * half_separators) % full_separators == 0)
                continue;

            s32 val = (i + 1) * half_separators;
            float x = ImLerp(bb.Min.x, bb.Max.x, val / (float)max_value);

            #if 0
            draw_list->AddLine(ImVec2(x, ImLerp(bb.Min.y, bb.Max.y, 0.25f)), ImVec2(x, ImLerp(bb.Min.y, bb.Max.y, 0.75f)), separator_color);
            #else
            draw_list->AddLine(ImVec2(x, bb.Min.y), ImVec2(x, ImLerp(bb.Min.y, bb.Max.y, 0.25f)), separator_color);
            draw_list->AddLine(ImVec2(x, bb.Max.y), ImVec2(x, ImLerp(bb.Min.y, bb.Max.y, 0.75f)), separator_color);
            #endif
        }
    }

    char overlay_buf[32]; 
    ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%d / %d", current_value, max_value);

    ImVec2 overlay_size = ImGui::CalcTextSize(overlay_buf, NULL);
    ImGui::RenderTextClipped(bb.Min, bb.Max, overlay_buf, 0, &overlay_size, ImVec2(0.5f, 0.5f), &bb);

}

void crafting_result_display(int32_t current_progress,
                             int32_t current_quality,
                             int32_t current_cp,
                             int32_t current_durability,

                             int32_t max_progress,
                             int32_t max_quality,
                             int32_t max_cp,
                             int32_t max_durability) {
    auto input_str_width = global_data.input_str_width;

    ImGui::Text("Progress");
    SetNextItemMaxWidth(input_str_width);
    FFUI_ProgressBar(current_progress, max_progress, 0xFF4AA24A, 0xFF39D3B5);

    ImGui::Text("Quality");
    SetNextItemMaxWidth(input_str_width);
    FFUI_ProgressBar(current_quality, max_quality, 0xFFCE7152, 0xFFA5D339);

    ImGui::Text("CP");
    SetNextItemMaxWidth(input_str_width);
    FFUI_ProgressBar(current_cp, max_cp, 0xFF640A46, 0xFFC263A4);


    u32 durability_color = 0xFF9F99FF;
    if (current_durability > max_durability / 4)
        durability_color = 0xFF6DCAFF;
    if (current_durability > max_durability / 2)
        durability_color = 0xFFFFCA6D;

    ImGui::Text("Durability");
    SetNextItemMaxWidth(input_str_width);
    FFUI_ProgressBar(current_durability, max_durability, durability_color, durability_color, 10, 5);
}


void setup_panel() {
    auto input_int_width = global_data.input_int_width;
    auto &data = global_data.craft_setup;

    job_selector(data.selected_job);
    if (data.selected_job == (Craft_Job)-1)
        return;

    auto &profile = data.job_settings[data.selected_job];

    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(input_int_width);
    ImGui::InputInt("Level", &profile.level);
    profile.level = clamp(profile.level, 1, 90);

    ImGui::SetNextItemWidth(input_int_width);
    ImGui::InputInt("Craftsmanship", &profile.craftsmanship);
    profile.craftsmanship = clamp(profile.craftsmanship, 0, 9999);

    ImGui::SetNextItemWidth(input_int_width);
    ImGui::InputInt("Control", &profile.control);
    profile.control = clamp(profile.control, 0, 9999);

    ImGui::SetNextItemWidth(input_int_width);
    ImGui::InputInt("CP", &profile.cp);
    profile.cp = clamp(profile.cp, 0, 9999);

    FFUI_Checkbox("Specialist", &profile.specialist);

    ImGui::NewLine();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Activate:");

    if (FFUI_Button("None")) {
        profile.active_actions = 0;
    }

    if (FFUI_Button("by Level")) {
        profile.active_actions = 0;

        for (int i = 0; i < NUM_ACTIONS; i++) {
            if (Craft_Actions_min_lvl[i] <= profile.level)
                profile.active_actions |= 1 << i;
        }
    }

    if (FFUI_Button("All")) {
        profile.active_actions = 0;

        for (int i = 0; i < NUM_ACTIONS; i++)
            profile.active_actions |= 1 << i;
    }

    craft_actions_selector(data.selected_job, profile.active_actions, ALL_ACTIONS);

    ImGui::EndGroup();
}

void simulator_panel() {
    auto &simulator_data = global_data.simulator;

    job_selector(simulator_data.selected_job);
    if (simulator_data.selected_job == (Craft_Job)-1)
        return;

    auto &profile = global_data.craft_setup.job_settings[simulator_data.selected_job];
    auto &data = simulator_data.per_job[simulator_data.selected_job];

    recipe_selector(simulator_data.selected_job, data.selected_recipe);
    if (!data.selected_recipe)
        return;

    auto context = Crafting_Simulator::init_context(
        profile.level,
        profile.cp,
        profile.craftsmanship,
        profile.control,

        data.selected_recipe->level,
        data.selected_recipe->progress_divider,
        data.selected_recipe->progress_modifier,
        data.selected_recipe->quality_divider,
        data.selected_recipe->quality_modifier,

        data.selected_recipe->durability,
        data.selected_recipe->progress,
        data.selected_recipe->quality
    );

    int32_t num_possible_actions = 0;
    for (int i = 0; i < data.num_actions; i++) {
        Craft_Action action = data.actions[i];
        if (!Crafting_Simulator::simulate_step(context, action))
            break;

        num_possible_actions++;
    }

    crafting_result_display(data.selected_recipe->progress - context.state.z,
                            data.selected_recipe->quality - context.state.w,
                            context.state.x,
                            context.state.y,

                            data.selected_recipe->progress,
                            data.selected_recipe->quality,
                            profile.cp,
                            data.selected_recipe->durability);

    #define insert_at_index(i, action) do { memmove(data.actions + (i) + 1, data.actions + (i), data.num_actions - (i)); data.actions[(i)] = (action); data.num_actions++; } while(0)
    #define remove_at_index(i) do { memmove(data.actions + (i), data.actions + (i) + 1, data.num_actions - (i) - 1); data.num_actions--; } while(0)

    bool has_actions = data.num_actions != 0;
    Craft_Action previous_action = (Craft_Action)-1;
    for (int i = 0; i < data.num_actions; i++) {
        auto button = FFUI_register_ActionButton(ImGui::GetID(i), i != 0);

        if (ImGui::BeginDragDropSource()) {
            FFUI_draw_ActionButton_empty(button);
            FFUI_ActionButton_action(ImGui::GetID(i), simulator_data.selected_job, data.actions[i], previous_action);

            Drag_Drop_Payload_Craft_Action payload = {};
            payload.job = simulator_data.selected_job;
            payload.action = data.actions[i];
            payload.index_plus_one = i + 1;

            ImGui::SetDragDropPayload("Craft_Action", &payload, sizeof(payload));
            ImGui::EndDragDropSource();

            continue;
        }

        if (ImGui::BeginDragDropTarget()) {
            auto payload = ImGui::AcceptDragDropPayload("Craft_Action", ImGuiDragDropFlags_AcceptBeforeDelivery);
            if (payload) {
                auto payload_info = (Drag_Drop_Payload_Craft_Action *)payload->Data;

                FFUI_draw_ActionButton_action(button, payload_info->job, payload_info->action, previous_action);
                button = FFUI_register_ActionButton(ImGui::GetID(i));

                if (payload->Delivery) {
                    insert_at_index(i, payload_info->action);

                    if (payload_info->index_plus_one) {
                        if (i < payload_info->index_plus_one)
                            remove_at_index(payload_info->index_plus_one);
                        else
                            remove_at_index(payload_info->index_plus_one - 1);
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        FFUI_draw_ActionButton_action(button, simulator_data.selected_job, data.actions[i], previous_action, i < num_possible_actions);

        if (button.pressed)
            remove_at_index(i);

        previous_action = data.actions[i];
    }

    auto empty_slot = FFUI_register_ActionButton(ImGui::GetID(-1), has_actions);

    if (ImGui::BeginDragDropTarget()) {
        auto payload = ImGui::AcceptDragDropPayload("Craft_Action", ImGuiDragDropFlags_AcceptBeforeDelivery);
        if (payload) {
            auto payload_info = (Drag_Drop_Payload_Craft_Action *)payload->Data;

            FFUI_draw_ActionButton_action(empty_slot, payload_info->job, payload_info->action, previous_action);
            empty_slot = FFUI_register_ActionButton(ImGui::GetID(-1));

            if (payload->Delivery) {
                data.actions[data.num_actions] = payload_info->action;
                data.num_actions++;

                if (payload_info->index_plus_one)
                    remove_at_index(payload_info->index_plus_one - 1);
            }
        }

        ImGui::EndDragDropTarget();
    }

    FFUI_draw_ActionButton_empty(empty_slot);


    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Copy to Clipboard: ");

    if (FFUI_Button("Teamcraft"))
        copy_actions_to_clipboard_teamcraft(data.actions, data.num_actions);

    char macro_buf[16] = {};
    int actions_left = data.num_actions;
    int current_macro = 0;

    while (actions_left) {
        snprintf(macro_buf, sizeof(macro_buf), data.num_actions > global_data.lines_per_macro ? "Macro %d" : "Macro", current_macro + 1);
        if (FFUI_Button(macro_buf))
            copy_actions_to_clipboard_macro(data.actions + current_macro * global_data.lines_per_macro, clamp(actions_left, 0, global_data.lines_per_macro));

        current_macro++;
        actions_left -= clamp(global_data.lines_per_macro, 0, actions_left);
    }


    u32 active_actions = profile.active_actions;
    u32 visible_actions = active_actions;

    for (int i = 0; i < NUM_ACTIONS; i++) {
        auto ctx = context;
        if(!Crafting_Simulator::simulate_step(ctx, (Craft_Action)i))
            active_actions &= ~(1 << i);
    }

    u32 possible_actions = active_actions;
    craft_actions_selector(simulator_data.selected_job, active_actions, visible_actions, data.num_actions ? data.actions[data.num_actions - 1] : (Craft_Action)-1, true);
    
    u32 changed_flag = possible_actions ^ active_actions;
    if (changed_flag) {
        if (data.num_actions == MAX_SIM_ACTIONS)
            return;

        DWORD action = 0;
        _BitScanForward(&action, changed_flag);
        data.actions[data.num_actions] = (Craft_Action)action;
        data.num_actions++;
    }
}

volatile u32 solvers_running;
volatile u32 solver_context_generation;
Crafting_Solver3::Solver_Context solver_context;

struct Solver_Worker_Info {
    u64 seed;
    u32 result_context_generation;
    Crafting_Solver3::Result result;
};

Solver_Worker_Info solver_worker_infos[MAX_NUM_SOLVER_THREADS];

Crafting_Solver3::Result get_current_best_solver_result() {
    u32 context_generation = solver_context_generation;
    Crafting_Solver3::Result result = solver_context.current_best;

    for (int i = 0; i < MAX_NUM_SOLVER_THREADS; i++) {
        if (solver_worker_infos[i].result_context_generation != context_generation)
            continue;

        Crafting_Solver3::Result solver_result = solver_worker_infos[i].result;
        if (Crafting_Solver3::b_better_than_a(result, solver_result))
            result = solver_result;
    }

    return result;
}

void set_new_solver_context(Crafting_Solver3::Solver_Context &context) {
    solver_context = context;
    InterlockedIncrement(&solver_context_generation);
}

void solver_panel() {
    auto &solver = global_data.solver;
    static u32 seed = 0;

    job_selector(solver.selected_job);
    if (solver.selected_job == (Craft_Job)-1)
        return;

    auto &profile = global_data.craft_setup.job_settings[solver.selected_job];
    auto &data = solver.per_job[solver.selected_job];

    if (recipe_selector(solver.selected_job, data.selected_recipe)) {
        data.current_best.depth = INT32_MAX;
        data.current_best.state = s32x4(0, 0, INT_MAX, INT_MAX);
        seed = 0;

        Crafting_Solver3::Solver_Context context = Crafting_Solver3::init_solver_context(
            seed,
            profile.active_actions,
            profile.level,
            profile.cp,
            profile.craftsmanship,
            profile.control,

            data.selected_recipe->level,
            data.selected_recipe->progress_divider,
            data.selected_recipe->progress_modifier,
            data.selected_recipe->quality_divider,
            data.selected_recipe->quality_modifier,

            data.selected_recipe->durability,
            data.selected_recipe->progress,
            data.selected_recipe->quality
        );

        set_new_solver_context(context);
    }
    if (!data.selected_recipe)
        return;

    if (!solvers_running) {
        if (FFUI_Button("Start", false))
            solvers_running = 1;
    } else {
        seed++;
        if (FFUI_Button("Pause", false))
            solvers_running = 0;
    }

    int max_progress = data.selected_recipe->progress;
    int max_quality = data.selected_recipe->quality;

    auto new_best = get_current_best_solver_result();
    data.current_best = Crafting_Solver3::b_better_than_a(data.current_best, new_best) ? new_best : data.current_best;

    auto current_result = data.current_best;

    int current_progress = max_progress - current_result.state.z;
    int current_quality = max_quality - current_result.state.w;

    crafting_result_display(current_progress,
                            current_quality,
                            current_result.state.x,
                            current_result.state.y,

                            data.selected_recipe->progress,
                            data.selected_recipe->quality,
                            profile.cp,
                            data.selected_recipe->durability);

    if (!current_result.depth)
        FFUI_draw_ActionButton_empty(FFUI_register_ActionButton(ImGui::GetID(0), false));

    Craft_Action previous_action = (Craft_Action)-1;
    for (int i = 0; i < current_result.depth; i++) {
        FFUI_ActionButton_action(ImGui::GetID(i), solver.selected_job, current_result.actions[i], previous_action, true, false, false, i != 0);
        previous_action = current_result.actions[i];
    }

    if (FFUI_Button("Edit in Simulator", false, true)) {
        auto &sim = global_data.simulator;
        sim.selected_job = solver.selected_job;
        sim.per_job[sim.selected_job].num_actions = current_result.depth;
        sim.per_job[sim.selected_job].selected_recipe = data.selected_recipe;
        memcpy(sim.per_job[sim.selected_job].actions, current_result.actions, current_result.depth * sizeof(*current_result.actions));
        global_data.current_main_panel = Main_Panel_Crafting_Simulator;
    }

    if (current_result.depth) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Copy to Clipboard: ");

        if (FFUI_Button("Teamcraft"))
            copy_actions_to_clipboard_teamcraft(current_result.actions, current_result.depth);

        char macro_buf[16] = {};
        int actions_left = current_result.depth;
        int current_macro = 0;

        while (actions_left) {
            snprintf(macro_buf, sizeof(macro_buf), current_result.depth > global_data.lines_per_macro ? "Macro %d" : "Macro", current_macro + 1);
            if (FFUI_Button(macro_buf))
                copy_actions_to_clipboard_macro(current_result.actions + current_macro * global_data.lines_per_macro, clamp(actions_left, 0, global_data.lines_per_macro));

            current_macro++;
            actions_left -= clamp(global_data.lines_per_macro, 0, actions_left);
        }
    }
}

bool GUI::per_frame() {
    ImGuiStyle &style = ImGui::GetStyle();

    global_data.button_size = ImGui::GetFrameHeight() * 2;
    global_data.input_int_width = (ImGui::GetFrameHeight() + style.ItemInnerSpacing.x) * 2 + ImGui::GetFrameHeight() * 3;
    global_data.input_str_width = (ImGui::GetFrameHeight() + style.ItemInnerSpacing.x) * 2 + ImGui::GetFrameHeight() * 16;

    auto &current_main_panel = global_data.current_main_panel;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, style.FramePadding.y * 2));
    if (ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleVar();

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.629f, 0.523f, 0.306f, 1.000f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.808f, 0.667f, 0.388f, 1.000f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.904f, 0.833f, 0.694f, 1.000f));
        if (AlignedMenuItem("Profile", current_main_panel == Main_Panel_Profile)) current_main_panel = Main_Panel_Profile;
        if (AlignedMenuItem("Simulator", current_main_panel == Main_Panel_Crafting_Simulator)) current_main_panel = Main_Panel_Crafting_Simulator;
        if (AlignedMenuItem("Solver", current_main_panel == Main_Panel_Crafting_Solver)) current_main_panel = Main_Panel_Crafting_Solver;
        ImGui::PopStyleColor(3);

        ImGui::EndMainMenuBar();
    }

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    if (ImGui::Begin("Main Panel", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        if (global_data.current_main_panel == Main_Panel_Profile) {
            setup_panel();
        } else if (global_data.current_main_panel == Main_Panel_Crafting_Simulator) {
            simulator_panel();
        } else if (global_data.current_main_panel == Main_Panel_Crafting_Solver) {
            solver_panel();
        }

    } ImGui::End();

    ImGui::PopStyleVar();

    return false;
}

void solver_worker(Solver_Worker_Info *info) {
    u64 context_generation = 0;
    Crafting_Solver3::Solver_Context context; 
    Crafting_Solver3::Result result;

    u64 iterations_with_current_seed = 0;

    for (;;) {
        Sleep(500);

        while (solvers_running) {
            u32 new_context_generation = solver_context_generation;
            if (new_context_generation != context_generation) {
                context_generation = new_context_generation;
                context = solver_context;
                result.depth = 0;
                result.state = context.sim_context.state;
                Crafting_Solver3::re_seed(context, info->seed);
            }

            for (int i = 0; i < 50000; i++)
                Crafting_Solver3::execute_round(context);
            iterations_with_current_seed++;

            if (iterations_with_current_seed >= 500) {
                Crafting_Solver3::re_seed(context, (context.rng_inc >> 1) + MAX_NUM_SOLVER_THREADS);
                iterations_with_current_seed = 0;
            }

            if (context_generation != info->result_context_generation|| Crafting_Solver3::b_better_than_a(info->result, context.current_best)) {
                info->result = context.current_best;
                InterlockedExchange(&info->result_context_generation, context_generation);
            }
        }
    }
}

#include <thread>

const char *GUI::init(ID3D11Device *device) {
    if (!decompress_all_data())
        return "Failed to decompress required data.";

    u32 num_solver_threads_to_start = std::min(std::max(1U, std::thread::hardware_concurrency() * 3 / 4), MAX_NUM_SOLVER_THREADS);
    printf("INFO: Starting %u solver threads.\n", num_solver_threads_to_start);

    for (int i = 0; i < num_solver_threads_to_start; i++) {
        solver_worker_infos[i].seed = i;
        std::thread(solver_worker, &solver_worker_infos[i]).detach();
    }

    ImGui::StyleColorsDark();
    auto &io = ImGui::GetIO();
    auto &context = *ImGui::GetCurrentContext();
    auto &style = ImGui::GetStyle();
    auto &colors = style.Colors;

    colors[ImGuiCol_WindowBg] =       ImVec4(0.192f, 0.188f, 0.192f, 1.000f);
    colors[ImGuiCol_MenuBarBg] =      ImVec4(0.192f, 0.192f, 0.192f, 1.000f);

    colors[ImGuiCol_FrameBg] =        ImVec4(0.333f, 0.333f, 0.333f, 1.000f);
    colors[ImGuiCol_TextDisabled] =   ImVec4(0.549f, 0.549f, 0.549f, 1.000f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.408f, 0.824f, 0.000f, 0.390f);

    colors[ImGuiCol_Button] =         ImVec4(0.290f, 0.290f, 0.290f, 1.000f);
    colors[ImGuiCol_ButtonHovered] =  ImVec4(0.353f, 0.353f, 0.353f, 1.000f);
    colors[ImGuiCol_ButtonActive] =   ImVec4(0.415f, 0.415f, 0.415f, 1.000f); 

    // ImGui::Selectable
    colors[ImGuiCol_Header] =         ImVec4(0.290f, 0.290f, 0.290f, 1.000f);
    colors[ImGuiCol_HeaderHovered] =  ImVec4(0.353f, 0.353f, 0.353f, 1.000f);
    colors[ImGuiCol_HeaderActive] =   ImVec4(0.415f, 0.415f, 0.415f, 1.000f);


    io.IniFilename = "FFTools.ini";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    io.Fonts->AddFontFromMemoryCompressedBase85TTF(Font_Roboto_compressed_data_base85, 16.0f);
    global_data.big_font = io.Fonts->AddFontFromMemoryCompressedBase85TTF(Font_Roboto_compressed_data_base85, 32.0f);

    if (!(global_data.actions_texture = create_texture(device, CA_TEXTURE_WIDTH, CA_TEXTURE_HEIGHT, (DXGI_FORMAT)CA_TEXTURE_FORMAT, CA_TEXTURE_PIXEL_DATA, CA_TEXTURE_WIDTH * 4, CA_TEXTURE_ARRAY_SIZE, CA_TEXTURE_MIP_LEVELS)))
        return "Failed to upload actions texture to GPU.";

    ImGuiSettingsHandler profile_handler = {};
    profile_handler.TypeName = "Profile";
    profile_handler.TypeHash = ImHashStr("Profile");
    profile_handler.ReadOpenFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler, const char *name) -> void *{
        for (int i = 0; i < NUM_JOBS; i++)
            if (strcmp(name, Craft_Job_to_string[i]) == 0)
                return &global_data.craft_setup.job_settings[i];

        return 0;
    };
    profile_handler.ReadLineFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler, void *entry, const char *line) {
        auto &profile = *(Global_Data::Craft_Setup::Profile *)entry;
        int32_t i32;
        String line_str = { (int64_t)strlen(line), (char *)line };

        if (sscanf(line, "level=%i", &i32) == 1) { profile.level = i32; } 
        else if (sscanf(line, "craftsmanship=%i", &i32) == 1) { profile.craftsmanship = i32; } 
        else if (sscanf(line, "control=%i", &i32) == 1) { profile.control = i32; } 
        else if (sscanf(line, "cp=%i", &i32) == 1) { profile.cp = i32; } 
        else if (sscanf(line, "specialist=%i", &i32) == 1) { profile.specialist = i32 ? true : false; }
        else if (starts_with(line_str, "active_action=")) { 
            advance(line_str, "active_action="_s.length); 
            for (int i = 0; i < NUM_ACTIONS; i++) {
                if (line_str == Craft_Action_to_serialization_string[i]) {
                    profile.active_actions |= (1 << i);
                    return;
                }
            }
            printf("WARNING: Unknown action in save file. It will be dropped on the next save.\n");
        }
    };
    profile_handler.WriteAllFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf) {
        for (int i = 0; i < NUM_JOBS; i++) {
            auto &data = global_data.craft_setup.job_settings[i];
            buf->appendf("[%s][%s]\n", handler->TypeName, Craft_Job_to_string[i].data);
            buf->appendf("level=%d\n", data.level);
            buf->appendf("craftsmanship=%d\n", data.craftsmanship);
            buf->appendf("control=%d\n", data.control);
            buf->appendf("cp=%d\n", data.cp);
            buf->appendf("specialist=%d\n", data.specialist);
            for (int i = 0; i < NUM_ACTIONS; i++) {
                if (data.active_actions & (1 << i))
                    buf->appendf("active_action=%s\n", Craft_Action_to_serialization_string[i].data);
            }
            buf->append("\n");
        }
    };
    context.SettingsHandlers.push_back(profile_handler);

    return 0;
}