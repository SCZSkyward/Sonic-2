#include "App.h"
//#include <SDL3/SDL_image.h>
#include <SDL_image.h>
#include <cmath>
#include <nfd.hpp>
#include <iostream>

// Handle input to the custom textboxes
void handleTextboxInput(OptionItem* selectedItem, Zone* currentZone, bool save) {
	if (selectedItem->name == "ZoneName") {
		if (save) currentZone->zoneName = selectedItem->text;
		else selectedItem->text = currentZone->zoneName;
	}
	else if (selectedItem->name == "ActNo") {
		if (save) {
			selectedItem->text.erase(0, selectedItem->text.find_first_not_of('0'));
			if (selectedItem->text.size() == 0 || selectedItem->text == "0") selectedItem->text = "1";
			currentZone->actNo = stoi(selectedItem->text);
		}
		else {
			selectedItem->text = std::to_string(currentZone->actNo);
		}
	}

	selectedItem->updateText();
}

// Get the position of the selected tile in the options bar
int* getActiveTilePos(int tile, int tileScreenSize) {
	int pos[2];

	pos[0] = (tile % 20) * tileScreenSize + settings.getScreenWidth() + settings.MENU_PADDING;
	pos[1] = (tile / 20) * tileScreenSize + settings.MENU_PADDING;

	return pos;
}

App::App()
{
	_running = true;

	//settings = new Settings(1200, 700);

	//setScreenSizes(1200, 700);
	updateTileScreenSize();

	NFD::Guard nfdGuard;

	SDL_Init(SDL_INIT_VIDEO);
	window = SDL_CreateWindow("Sonic 2 Level Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, settings.getWindowWidth(), settings.getWindowHeight(), SDL_WINDOW_RESIZABLE);
	renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, settings.getWindowWidth(), settings.getWindowHeight());

	SDL_SetWindowMinimumSize(window, settings.MIN_WINDOW_WIDTH, settings.MIN_WINDOW_HEIGHT);
	//SDL_SetEventFilter(resizeWindow, window);

	TTF_Init();
	this->font = TTF_OpenFont("../sonic-the-hedgehog-2-hud-font.ttf", 30);

	this->gameRenderer = new Renderer(renderer);
	this->optionMenu = new OptionMenu(renderer, font, {0, 0, 0, 120});

	camX = 0; camY = 0;
	tileSize = 16;

	selectedItem = nullptr;

	loadDefaultZone();
	
	optionMenu->addMenuItem(TextInput, "ZoneName", currentZone->zoneName, "Zone Name:");
	optionMenu->addMenuItem(NumberInput, "ActNo", std::to_string(currentZone->actNo), "Act Number:");
	optionMenu->addMenuItem(Button, "SaveButton", "Save");
	optionMenu->addMenuItem(Button, "LoadButton", "Load", "", true);
	optionMenu->addMenuItem(Button, "NewButton", "New", "", true);

	onExecute();
}

int App::onExecute()
{
	float timeA, timeB = 0;

	SDL_Event event;
	while (_running) {
		while (SDL_PollEvent(&event)) {
			onEvent(&event);
		}

		timeA = SDL_GetTicks();
		float deltaTime = timeA - timeB;
		if (deltaTime > 1000 / 20.0) {
			timeB = timeA;
			onLoop();
			onRender();
		}
	}

	onCleanup();
	return 0;
}

void App::onEvent(SDL_Event* event)
{
	switch (event->type) {
	case SDL_QUIT:
		_running = false;
		break;
	case SDL_KEYDOWN:
		keyboard[event->key.keysym.sym] = true;
		if (selectedItem != nullptr) selectedItem->onType(event->key.keysym.sym);
		break;
	case SDL_KEYUP:
		keyboard[event->key.keysym.sym] = false;
		break;
	case SDL_MOUSEBUTTONDOWN:
		mouse[event->button.button] = true;
		break;
	case SDL_MOUSEBUTTONUP:
		mouse[event->button.button] = false;
		break;
	case SDL_MOUSEMOTION:
		mouseX = event->motion.x;
		mouseY = event->motion.y;
		movementX = event->motion.xrel;
		movementY = event->motion.yrel;
		break;
	case SDL_MOUSEWHEEL:
		mouseWheel = event->wheel.y;
		break;
	case SDL_WINDOWEVENT:
		if (event->window.event == SDL_WINDOWEVENT_RESIZED) {
			// Get screen sizes before updating them, and after. Used for updating the options menu
			float oldSizes[3] = { 
				settings.getWindowHeight(), 
				settings.getScreenWidth(), 
				settings.getOptionsWidth() 
			
			};
			settings.setScreenSizes(event->window.data1, event->window.data2);

			float newSizes[3] = { 
				settings.getWindowHeight(), 
				settings.getScreenWidth(), 
				settings.getOptionsWidth() 
			};

			optionMenu->updateTileSetRect(newSizes);
			for (OptionItem item : optionMenu->options) {
				item.updatePosition(oldSizes, newSizes);
			}

			updateTileScreenSize();
		}
		break;
	}
}

void App::onLoop()
{
	// Default cursor
	SDL_Cursor* cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);

	// If hovering over an option in the menu, set it as hovered and change the cursor.
	for (int i = 0; i < optionMenu->options.size(); i++) {
		if (optionMenu->options[i].type == Button || optionMenu->options[i].type == TextInput || optionMenu->options[i].type == NumberInput) {
			if (mouseX >= optionMenu->options[i].rect->x && mouseX <= optionMenu->options[i].rect->x + optionMenu->options[i].rect->w
				&& mouseY >= optionMenu->options[i].rect->y && mouseY <= optionMenu->options[i].rect->y + optionMenu->options[i].rect->h) {
				cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
				optionMenu->options[i].hovered = true;
			}
			else optionMenu->options[i].hovered = false;
		}
	}

	// Check if a menu option is selected
	if (selectedItem == nullptr) {
		// Camera movement using keyboard keys
		if (keyboard[SDLK_a] || keyboard[SDLK_LEFT]) camX += tileSize;
		if (keyboard[SDLK_d] || keyboard[SDLK_RIGHT]) camX -= tileSize;
		if (keyboard[SDLK_w] || keyboard[SDLK_UP]) camY += tileSize;
		if (keyboard[SDLK_s] || keyboard[SDLK_DOWN]) camY -= tileSize;
	
		// Camera zoom using keyboard keys
		if (keyboard[SDLK_EQUALS] || mouseWheel > 0) if (tileSize < 64) tileSize++;
		if (keyboard[SDLK_MINUS] || mouseWheel < 0) if (tileSize > 16) tileSize--;
	}
	else {
		if (selectedItem->type == TextInput || selectedItem->type == NumberInput) {
			// If return is pressed, confirm edit to menu option
			if (keyboard[SDLK_RETURN]) {
				handleTextboxInput(selectedItem, currentZone, true);

				selectedItem->selected = false;
				selectedItem = nullptr;
			}
			// If escape is pressed, disgard edit to menu option
			if (keyboard[SDLK_ESCAPE]) {
				handleTextboxInput(selectedItem, currentZone, false);

				selectedItem->selected = false;
				selectedItem = nullptr;
			}
		}
		else if (selectedItem->type == Button) {
			if (selectedItem->name == "NewButton") {
				int result = 0;

				const SDL_MessageBoxButtonData buttons[] = { 
					{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, -1, "No"}, 
					{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes"} 
				};

				SDL_MessageBoxData boxData{ SDL_MESSAGEBOX_WARNING, window, "Confirm", "Are you Sure?", 2, buttons, NULL };
				SDL_ShowMessageBox(&boxData, &result);

				if (result == 1) loadDefaultZone();
			}
			else if (selectedItem->name == "SaveButton") {
				currentZone->saveZone();
			}
			else if (selectedItem->name == "LoadButton") {
				NFD::UniquePath outPath;
				nfdfilteritem_t filterItem[1] = {"Zone File", "zone"};
				nfdresult_t result = NFD::OpenDialog(outPath, filterItem, 1);
				if (result == NFD_OKAY) {
					//std::cout << "Success!" << std::endl << outPath.get() << std::endl;

					currentZone = Zone::OpenZone(renderer, outPath.get());
				}
			}
			//TODO: sort out button inputs for individual buttons.
			selectedItem->selected = false;
			selectedItem = nullptr;
		}
	}
	
	if (mouse[SDL_BUTTON_LEFT]) {
		// Check if an option has been clicked. If it has, then if it can be selected, select it.
		// Else, if anywhere else is clicked then unselect any selected items.
		for (int i = 0; i < optionMenu->options.size(); i++) {
			if (optionMenu->options[i].hovered) {
				if (optionMenu->options[i].type == TextInput || optionMenu->options[i].type == NumberInput 
					|| optionMenu->options[i].type == Button) {
					selectedItem = &optionMenu->options[i];
					selectedItem->selected = true;
					break;
				}
			}
			else {
				if (selectedItem != nullptr) {
					handleTextboxInput(selectedItem, currentZone, true);

					selectedItem->selected = false;
					selectedItem = nullptr;
				}
			}
		}

		// If a tile is clicked on the tilemap within the options menu, then set it as the active drawing tile.
		for (int x = 0; x < 20; x++) {
			for (int y = 0; y < 20; y++) {
				int xPos = (x * tileScreenSize) + settings.getScreenWidth() + 20;
				int yPos = (y * tileScreenSize) + 20;
				if (mouseX >= xPos && mouseX < xPos + tileScreenSize
					&& mouseY >= yPos && mouseY < yPos + tileScreenSize) {
					activeTile = x + (y * 20);
				}
			}
		}

		// If the mouse is clicked outside of the option menu, place a tile at the selected area.
		if (mouseX < settings.getScreenWidth()) {
			for (int x = 0; x < currentZone->zoneWidth; x++) {
				for (int y = 0; y < currentZone->zoneHeight; y++) {
					int xPos = (x * tileSize) + camX;
					int yPos = (y * tileSize) + camY;
					if (mouseX >= xPos && mouseX < xPos + tileSize
						&& mouseY >= yPos && mouseY < yPos + tileSize) {
						currentZone->mapSet[x + (y * currentZone->zoneWidth)] = {
							true, 
							(SDL_GetModState() & KMOD_LSHIFT) ? true : false,
							(SDL_GetModState() & KMOD_LCTRL) ? true : false,
							0, 
							activeTile
						};

						goto endL;
					}
				}
			}
		}

		// If mouse is being held in the main screen, and is moved to the menu, unclick the mouse
		endL:
		mouse[SDL_BUTTON_LEFT] = false;
	}
	if (mouse[SDL_BUTTON_RIGHT]) {
		// If the mouse is clicked outside of the option menu, remove a tile at the selected area.
		if (mouseX < settings.getScreenWidth()) {
			for (int x = 0; x < currentZone->zoneWidth; x++) {
				for (int y = 0; y < currentZone->zoneHeight; y++) {
					int xPos = (x * tileSize) + camX;
					int yPos = (y * tileSize) + camY;
					if (mouseX >= xPos && mouseX < xPos + tileSize
						&& mouseY >= yPos && mouseY < yPos + tileSize) {
						Tile* currentTile = &currentZone->mapSet[x + (y * currentZone->zoneWidth)];
						currentTile->reset();

						goto endR;
					}
				}
			}
		}

		// If mouse is being held in the main screen, and is moved to the menu, unclick the mouse
		endR:
		mouse[SDL_BUTTON_RIGHT] = false;
	}
	if (mouse[SDL_BUTTON_MIDDLE]) {
		// if the mouse is within the bounds of the editor (not including options)
		// then move the camera by the amount dragged by the mouse.
		if (mouseX > 0 && mouseX < settings.getScreenWidth()
			&& mouseY > 0 && mouseY < settings.getWindowHeight()) {
			cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
			camX += tileSize * movementX * 2;
			camY += tileSize * movementY * 2;
		}
	}

	// Change the cursor when hovering over the tileset in the options menu
	if (mouseX >= settings.getScreenWidth() + 20 && mouseX <= settings.getScreenWidth() + settings.getOptionsWidth() - 20
		&& mouseY >= 20 && mouseY <= settings.getOptionsWidth() - 20) {
			cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	}

	// Limit the bounds of the camera on the X and Y axis so you can't move too far from the level.
	if (camX > tileSize * 10) camX = tileSize * 10;
	else if (camX < -((currentZone->zoneWidth * tileSize) + (tileSize * 10) - settings.getScreenWidth()))
		camX = -((currentZone->zoneWidth * tileSize) + (tileSize * 10) - settings.getScreenWidth());

	if (camY > tileSize * 10) camY = tileSize * 10;
	else if (camY < -((currentZone->zoneHeight * tileSize) + (tileSize * 10) - settings.getWindowHeight()))
		camY = -((currentZone->zoneHeight * tileSize) + (tileSize * 10) - settings.getWindowHeight());

	// Reset movement/zoom vectors for the next frame.
	mouseWheel = 0; movementX = 0; movementY = 0;
	SDL_SetCursor(cursor);
}

void App::onRender()
{
	SDL_SetRenderDrawColor(renderer, 
		currentZone->backgroundColor.r, currentZone->backgroundColor.g, 
		currentZone->backgroundColor.b, currentZone->backgroundColor.a);
	SDL_RenderClear(renderer);

	currentZone->renderZone(camX, camY, tileSize);

	// Render the options menu background/border.
	gameRenderer->renderFilledRect({ settings.getScreenWidth(), 0, settings.getOptionsWidth(), settings.getWindowHeight()}, currentZone->backgroundColor);
	gameRenderer->renderRect({ settings.getScreenWidth(), 0, settings.getOptionsWidth(), settings.getWindowHeight()}, {0, 0, 0, 120});

	optionMenu->renderTileSet(currentZone->tileSet);

	for (OptionItem option : optionMenu->options) {
		option.render();
	}

	// Render a rect over the selected tile to give indication it is selected.
	int* activeTilePos = getActiveTilePos(activeTile, tileScreenSize);
	gameRenderer->renderRect({ activeTilePos[0], activeTilePos[1], tileScreenSize, tileScreenSize }, { 255, 255, 255 ,255 });

	// when hovering over a tile in the options menu, highlight it.
	for (int x = settings.getScreenWidth() + settings.MENU_PADDING; x < settings.getWindowWidth() - settings.MENU_PADDING; x += tileScreenSize) {
		for (int y = settings.MENU_PADDING; y < settings.getOptionsWidth() - settings.MENU_PADDING; y += tileScreenSize) {
			if (mouseX >= x && mouseX < x + tileScreenSize
				&& mouseY >= y && mouseY < y + tileScreenSize) {
				gameRenderer->renderFilledRect({ x, y, tileScreenSize, tileScreenSize }, { 255, 255, 255 ,120 });
			}
		}
	}

	SDL_RenderPresent(renderer);
}

void App::onCleanup()
{
	TTF_Quit();

	SDL_DestroyTexture(currentTileSet);
	SDL_DestroyRenderer(renderer);

	SDL_DestroyWindow(window);
	SDL_Quit();
}

void App::loadDefaultZone()
{
	currentZone = new Zone(renderer, "Zone Name", 1, { 23, 27, 33, 255 }, "Emerald_Hill.png");
	currentTileSet = currentZone->tileSet;
	for (int i = 0; i < optionMenu->options.size(); i++) {
		if (optionMenu->options[i].name == "ZoneName") optionMenu->options[i].text = currentZone->zoneName;
		else if (optionMenu->options[i].name == "ActNo") optionMenu->options[i].text = std::to_string(currentZone->actNo);
		optionMenu->options[i].updateText();
	}
}

void App::updateTileScreenSize()
{
	tileScreenSize = (settings.getOptionsWidth() - (settings.MENU_PADDING * 2)) / 20;
}

int main(int argc, char* argv[]) {
	//Settings(1200, 700);
	App app;

	return app.onExecute();
}