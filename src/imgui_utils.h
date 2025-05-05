#pragma once

#include <imgui/imgui_internal.h>

inline ImVec2 ImConv(const float2& v) { return {v.x, v.y}; }
inline ImVec4 ImConv(const float4& v) { return {v.x, v.y, v.z, v.w}; }

inline void MinMax(ImVec2& a, ImVec2& b)
{
	ImVec2 ta = a;
	ImVec2 tb = b;
	a.x = std::min(ta.x, tb.x);
	a.y = std::min(ta.y, tb.y);
	b.x = std::max(ta.x, tb.x);
	b.y = std::max(ta.y, tb.y);
}

inline IntRect GetRect(const ImVec2& a, const ImVec2& b)
{
	IntRect r;
	r.Left   = (int)std::min(a.x, b.x);
	r.Top    = (int)std::min(a.y, b.y);
	r.Right  = (int)std::max(a.x, b.x);
	r.Bottom = (int)std::max(a.y, b.y);
	return r;
}

inline void TextUnformattedWithWrap(const char* data, const char* data_end)
{
	ImGuiContext* Ctx = ImGui::GetCurrentContext();
	const bool need_backup = (Ctx->CurrentWindow->DC.TextWrapPos < 0.0f);
	if (need_backup)
		ImGui::PushTextWrapPos(0.0f);

	ImGui::TextUnformatted(data, data_end);

	if (need_backup)
		ImGui::PopTextWrapPos();
}

inline void ImGuiAutoScrollY()
{
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		ImGui::SetScrollHereY(1.0f);
}
