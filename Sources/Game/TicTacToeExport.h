#pragma once

// Authoring-time exporters: run by the game binary in TTT_EXPORT mode to persist the
// code-built animations and scene as editor-editable assets (.anim, .scn)
namespace TicTacToeExport
{
	// Builds all TicTacToe animation clips and saves them as Assets/TicTacToe/Animations/*.anim
	void ExportAnimations();

	// Saves the current (script-built) scene as Assets/TicTacToe.scn with meta
	void ExportScene();
}
