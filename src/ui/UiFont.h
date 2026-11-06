#pragma once
#include "raylib.h"
namespace loblox::ui {
// Atlas de texto rasterizado no tamanho efetivo de desenho. Ampliar um atlas
// unico de 32px era a causa do texto borrado em HUD, gizmo e menus, por isso o
// chamador pede o tamanho e desenha exatamente nesse tamanho.
Font Text(float size);
// Tamanho do atlas para um tamanho pedido (arredondado e limitado).
int FontPixels(float size);
// Graphics-thread resource, shared by native HUD, menu and gizmo readouts.
Font SystemFont();
void ReleaseSystemFont();
}
