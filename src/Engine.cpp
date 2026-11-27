/*
The CEngine Framework was found on :

http://gpwiki.org/index.php/SDL:Tutorials:Simple_Engine_Framework

It was written by Anders "Sion" Nissen and last updated 14. August 2005.

Content of this file is available under GNU Free Documentation License 1.2. :
http://www.gnu.org/copyleft/fdl.html

We modified the file to make it feet our requirements.

nxdk port: the game draws into an 800x600 surface which the platform layer
scales to the console's video mode (see platform.h). Input uses the SDL2
game controller API instead of SDLx's joystick numbering.
*/

#include "Engine.h"
#include "platform.h"

#ifdef _XBOX
#	include "XPad.h"
#endif

/** Default constructor. **/
CEngine::CEngine()
{
#ifdef _XBOX
	m_pPad = NULL;
	for (int i = 0; i < 16; i++) m_bPadKey[i] = false;
#endif

	m_lLastRender   = 0;
	m_lLastThink    = 0;
	//m_iFrameLenght  = 15; //Durée en millisecondes d'une frame.
    m_iFrameLenght=0;

	m_iWidth		= GAME_W;
	m_iHeight		= GAME_H;

	m_czTitle		= NULL;

	m_pScreen		= NULL;

	m_bMinimized		= false;
	m_bFullscreen       = false;
}

/** Destructor. **/
CEngine::~CEngine()
{
	SDL_Quit();
}

/** Creates the 800x600 surface the game draws into.
	The size is fixed: the platform layer scales it to the real video mode.
**/
void CEngine::SetSize(const int& iWidth, const int& iHeight)
{
	m_iWidth  = GAME_W;
	m_iHeight = GAME_H;
	if (m_pScreen == NULL)
		m_pScreen = SDL_CreateRGBSurfaceWithFormat(0, GAME_W, GAME_H, 32, COMPAT_SCREEN_FORMAT);
}

//Même chose que précédemment, mais en plein écran.
void CEngine::SetFullScreen(const int& iWidth, const int& iHeight)
{
	SetSize(iWidth, iHeight);
}

// The console is always full screen.
void CEngine::flipFullScreen(bool fullscreen)
{
}

/** Initialize SDL, the window and the additional data. **/
void CEngine::Init()
{
	Uint32 flags = SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER;
#ifndef NXDK
	flags |= SDL_INIT_VIDEO;       // host test build opens a window
#endif
#ifndef NO_SOUND_2H4U
	flags |= SDL_INIT_AUDIO;
#endif

	if ( SDL_Init( flags ) < 0 )
	{
		fprintf( stderr, "Impossible d'initialiser SDL: %s\n", SDL_GetError() );
		Platform_Exit( 1 );
	}

	Platform_Init();

    //Chargement des paramètres (et choix du mode vidéo).
    ParamInit(m_iWidth,m_iHeight);

	// If we fail, return error.
	if ( m_pScreen == NULL )
	{
		fprintf( stderr, "Impossible de définir la vidéo: %s\n", SDL_GetError() );
		Platform_Exit( 1 );
	}

#ifdef _XBOX
	// The pad is opened when SDL reports it (SDL_CONTROLLERDEVICEADDED),
	// but grab one now if it is already there.
	for (int i = 0; i < SDL_NumJoysticks() && m_pPad == NULL; i++)
		if (SDL_IsGameController(i))
			m_pPad = SDL_GameControllerOpen(i);
#endif

	AdditionalInit();
}

//Le jeu sera en fait appellé par le menu.
//Pour revenir au menu, on fera un return.
/** The main loop. **/
void CEngine::Start()
{
	m_lLastRender = SDL_GetTicks();
	m_lLastThink = SDL_GetTicks();
	m_bQuit = false;

	// Main loop: loop forever.
	while ( !m_bQuit )
	{
		// Handle controller and keyboard input
		HandleInput();

		if ( m_bMinimized ) {
			// Release some system resources if the app. is minimized.
		    SDL_Delay(100);
		} else {
			// Do some thinking
			DoThink();

			// Render stuff
			DoRender();

		}
        //Pas de Delay : Platform_Present attend la synchro verticale.
	}

	QuitScreen();    //Message "Quitting!" pendant le retour au tableau de bord.
	AdditionalEnd();
	End(); //Lorsque l'on quitte le programme.

}

void CEngine::Quit()
{
    m_bQuit = true;
}

void CEngine::End()
{
#ifndef NXDK
     SDL_Quit();
#endif
     //Sur Xbox, retour direct au tableau de bord (voir Platform_Exit).
     Platform_Exit(0);
}

#ifdef _XBOX
/** Turns a pad input into game key presses, only when its state changes.
	iSlot identifies the input, iKey1/iKey2 are the SDLK_ codes it sends.
**/
void CEngine::PadKey(int iSlot, bool bDown, int iKey1, int iKey2)
{
	if (bDown == m_bPadKey[iSlot])
		return;
	m_bPadKey[iSlot] = bDown;
	if (bDown)
	{
		KeyDown(iKey1);
		if (iKey2) KeyDown(iKey2);
	}
	else
	{
		KeyUp(iKey1);
		if (iKey2) KeyUp(iKey2);
	}
}

/** Reads the controller once per frame and maps it to the keys the
	original Xbox port used (see XBOX.README.txt for the controls).
**/
void CEngine::HandlePad()
{
	if (m_pPad == NULL)
		return;

	SDL_GameController* p = m_pPad;
	const int lx = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTX);
	const int ly = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTY);
	const int rx = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_RIGHTX);
	const int lt = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	const int rt = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

	const bool up    = SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_UP)    || ly < -JOY_DEADZONE * 3;
	const bool down  = SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_DOWN)  || ly >  JOY_DEADZONE * 3;
	const bool left  = SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_LEFT)  || lx < -JOY_DEADZONE * 3;
	const bool right = SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx >  JOY_DEADZONE * 3;

	// Paddle: triggers or right stick (j = left, k = right).
	const bool paddleLeft  = lt > JOY_TRIGGER_THRESHOLD || rx < -JOY_DEADZONE;
	const bool paddleRight = rt > JOY_TRIGGER_THRESHOLD || rx >  JOY_DEADZONE;

	// Tetromino / menu navigation. Up also rotates (b), like the original port.
	PadKey(0, up,    SDLK_UP, SDLK_b);
	PadKey(1, down,  SDLK_DOWN);
	PadKey(2, left,  SDLK_LEFT);
	PadKey(3, right, SDLK_RIGHT);
	PadKey(4, paddleLeft,  SDLK_j);
	PadKey(5, paddleRight, SDLK_k);

	PadKey(6, SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_A) != 0,     SDLK_RETURN, SDLK_SPACE);
	PadKey(7, SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_B) != 0,     SDLK_b);
	PadKey(8, SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_START) != 0, SDLK_p);
	PadKey(9, SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_BACK) != 0,  SDLK_ESCAPE);
}
#endif

/** Handles all controller inputs.
	@remark This function is called once per frame.
**/
void CEngine::HandleInput()
{
	// Poll for events, and handle the ones we care about.
	SDL_Event event;
	while ( SDL_PollEvent( &event ) )
	{
		switch ( event.type )
		{
#ifdef _XBOX
		case SDL_CONTROLLERDEVICEADDED:
			if (m_pPad == NULL)
				m_pPad = SDL_GameControllerOpen(event.cdevice.which);
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			if (m_pPad != NULL &&
			    SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_pPad)) == event.cdevice.which)
			{
				SDL_GameControllerClose(m_pPad);
				m_pPad = NULL;
				for (int i = 0; i < SDL_NumJoysticks() && m_pPad == NULL; i++)
					if (SDL_IsGameController(i))
						m_pPad = SDL_GameControllerOpen(i);
			}
			break;
#endif
		case SDL_KEYDOWN:
			if (!event.key.repeat)
				KeyDown( event.key.keysym.sym );
			break;

		case SDL_KEYUP:
			KeyUp( event.key.keysym.sym );
			break;

		case SDL_QUIT:
			m_bQuit = true;
			break;
		} // switch
	} // while (handling input)

#ifdef _XBOX
	HandlePad();
#endif
}

/** Handles the updating routine. **/
void CEngine::DoThink()
{
    long iElapsedThink = SDL_GetTicks() - m_lLastThink;
	m_lLastThink = SDL_GetTicks();

	Think( iElapsedThink );

}


/** Renders a frame and hands it to the platform layer, which scales it
	to the screen and waits for vertical sync.
**/
void CEngine::DoRender()
{
    long iElapsedRender = SDL_GetTicks() - m_lLastRender;

    if (iElapsedRender>=m_iFrameLenght)
    {
        m_lLastRender = SDL_GetTicks();
        Render(m_pScreen);

        Platform_Present( m_pScreen );
    }
}

/** Sets the title of the window
	@param czTitle A character array that contains the text that the window title should be set to.
**/

void CEngine::SetTitle(const char* czTitle)
{
	m_czTitle = czTitle;
}

/** Retrieve the title of the application window.
	@return The last set windows title as a character array.
**/
const char* CEngine::GetTitle()
{
	return m_czTitle;
}

// No window icon on the console.
void CEngine::SetIcon(const char* czChemin)
{
}

/** Retrieve the main screen surface.
	@return A pointer to the SDL_Surface surface
	@remark The surface is not validated internally.
**/
SDL_Surface* CEngine::GetSurface()
{
	return m_pScreen;
}
