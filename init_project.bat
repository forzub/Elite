@echo off
chcp 65001 >nul

echo === Initializing Elite-like 3D Game Project Structure ===

REM root
mkdir src

REM core
mkdir src\core
type nul > src\core\Application.h
type nul > src\core\Application.cpp
type nul > src\core\GameState.h
type nul > src\core\StateStack.h
type nul > src\core\StateStack.cpp
type nul > src\core\Time.h
type nul > src\core\Time.cpp

REM window
mkdir src\window
type nul > src\window\Window.h
type nul > src\window\Window.cpp

REM input
mkdir src\input
type nul > src\input\Input.h
type nul > src\input\Input.cpp

REM ui
mkdir src\ui
type nul > src\ui\UIContext.h
type nul > src\ui\MainMenuState.h
type nul > src\ui\MainMenuState.cpp
type nul > src\ui\TradeMenuState.h
type nul > src\ui\TradeMenuState.cpp
type nul > src\ui\NavigationMenuState.h
type nul > src\ui\NavigationMenuState.cpp

REM scene
mkdir src\scene
type nul > src\scene\EntityID.h
type nul > src\scene\Entity.h
type nul > src\scene\Scene.h
type nul > src\scene\Scene.cpp

REM physics
mkdir src\physics
type nul > src\physics\Collider.h
type nul > src\physics\SphereCollider.h
type nul > src\physics\SphereCollider.cpp
type nul > src\physics\Collision.h
type nul > src\physics\PhysicsWorld.h
type nul > src\physics\PhysicsWorld.cpp

REM render
mkdir src\render
type nul > src\render\Renderer.h
type nul > src\render\Renderer.cpp
type nul > src\render\Camera.h
type nul > src\render\Camera.cpp

REM game
mkdir src\game
type nul > src\game\SpaceState.h
type nul > src\game\SpaceState.cpp
type nul > src\game\Ship.h
type nul > src\game\Ship.cpp

REM entry point
type nul > src\main.cpp

echo.
echo === Project structure created successfully ===
echo === You can now start writing the engine ===
pause
