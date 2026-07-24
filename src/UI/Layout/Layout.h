#pragma once

#include "imgui.h"

namespace Layout {
void Begin();
void End();

void BeginContainer(float width);
void EndContainer();

void Space(float pixels);

void BeginContent();
void EndContent();

float Scale(float value);
ImVec2 Scale(float x, float y);

float Width();
float Height();

float Margin();
} // namespace Layout