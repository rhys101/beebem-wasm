/****************************************************************************/
/*                               Beebem                                     */
/*                               ------                                     */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/****************************************************************************/
/* Mike Wyatt and NRM's port to win32 - 7/6/97 */

#include <emscripten.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <SDL.h>


#ifdef WITH_UNIX_EXTRAS
#	include <sys/types.h>
#	include <dirent.h>
#	include <pwd.h>
#endif

#include <iostream>
#include <fstream>
#include "windows.h"

#include "6502core.h"
#include "beebmem.h"
#include "beebsound.h"
#include "sysvia.h"
#include "uservia.h"
#include "beebwin.h"
#include "disc8271.h"
#include "video.h"
#include "via.h"
#include "atodconv.h"
#include "disc1770.h"
#include "serial.h"
#include "tube.h"
#include "econet.h"	//Rob

#include "scsi.h"	// Dave: Needed for reset on break
#include "sasi.h"

#include "i86.h"
#include "teletext.h"
#include "z80mem.h"
#include "z80.h"

#include "line.h"	// SDL Stuff
#include "log.h"
#include "sdl.h"

#include <gui.h>

#include "beebem_pages.h"
#include "fake_registry.h"

#include "beebemrc.h"

// Can remove this (only needed to calc string hash)
//#include <gui_functions.h>


#ifdef MULTITHREAD
#undef MULTITHREAD
#endif

/* Make global reference to command line args
 */
int __argc = 0;
char **__argv = NULL;

int X11_CapsLock_Down;
Uint32 ticks;

// ARJ
int autoboot;


/* This needs to be fixed.. ----
 */
int Tmp_Command_Line_Fullscreen=0;
/* -----------------------------
 */

//+>
	// Load/Save path.
	extern char *cfg_LoadAndSavePath_ptr;
//<+


extern int TorchTube;
extern int Enable_Z80;
extern VIAState SysVIAState;
int DumpAfterEach=0;

unsigned char MachineType;
BeebWin *mainWin = NULL;

void i86_main(void);

//-- HINSTANCE hInst;
//-- HWND hCurrentDialog = NULL;

//-- DWORD iSerialThread,iStatThread; // Thread IDs
FILE *tlog;


//----------------------------------------------

int done = 0;
//int fullscreen = 0;
int showing_menu = 0;
EG_Window *displayed_window_ptr = NULL;


//void CLEAN_EXIT(void)
//{
//  /* Quit SDL
//   */
//  SDL_Quit();
//}


void SetActiveWindow(EG_Window *window_ptr)
{
	displayed_window_ptr = window_ptr;
}

int GetFullscreenState(void)
{
	bool fullscreen_val = false;

	if (mainWin!=NULL) fullscreen_val = mainWin->IsFullScreen();

//	return(fullscreen);
	return fullscreen_val;
}

int ToggleFullscreen(void)
{
//	if (fullscreen != 0)
//		fullscreen = 0;
//	else
//		fullscreen = 1;

	if (mainWin->IsFullScreen())
		mainWin->SetFullScreenToggle(false);
	else
		mainWin->SetFullScreenToggle(true);
	SetFullScreenTickbox(mainWin->IsFullScreen());

	Destroy_Screen();
	if (Create_Screen() != 1){
		qFATAL("Could not recreate SDL window!\n");
		exit(10);
	}

	/* Update GUI here so it's not missed anywhere - this is
	 * turning into such a bloody MESS...
	 */
	ClearWindowsBackgroundCacheAndResetSurface();
	ClearVideoWindow();

//	if (SDL_WM_ToggleFullScreen(screen_ptr) != 1)
//		EG_Log(EG_LOG_WARNING, dL"Could not toggle full-screen mode.", dR);

//	return(fullscreen);
	return mainWin->IsFullScreen();
}

void UnfullscreenBeforeExit(void)
{
	/* Hopefully this will fix that annoying bug where the mouse pointer
	 * vanishes on exit.  Is that me, or is it SDL?
	 */
//	if (fullscreen)
	if (mainWin->IsFullScreen())
		ToggleFullscreen();
}

void ShowingMenu(void)
{
	showing_menu = 1;
}

void NoMenuShown(void)
{
	showing_menu = 0;
}

void Quit(void)
{
	done=1;
}


void one_iter() {
	/* Main loop converted to SDL:
	 */


  //  	done = 0; do {

//--		if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) || mainWin->IsFrozen())
//--		{
//--			if(!GetMessage(&msg,    // message structure
//--							NULL,   // handle of window receiving the message
//--							0,      // lowest message to examine
//--							0))
//--				break;              // Quit the app on WM_QUIT
//--  
//--			if (hCurrentDialog == NULL || !IsDialogMessage(hCurrentDialog, &msg)) {
//--				TranslateMessage(&msg);// Translates virtual key codes
//--				DispatchMessage(&msg); // Dispatches message to window
//--			}
//--		}

		/* As X11 sucks, we need to release the Caps Lock key ourselves, an event will not happen.
		 * The X11_CapsLock_Down var basically counts down once per pass of the emulator core, when
		 * it's 1, we assume enough time has passed to pass the core a release of the Beebs Caps Lock
		 * key.
		 */
		if (X11_CapsLock_Down>0){
			X11_CapsLock_Down--;
			if (X11_CapsLock_Down == 0)
				BeebKeyUp(4, 0);
		}
		


		/* Toggle processing of either events to the emulator core, or events to the menu.
		 */
		SDL_Event event;

		if (showing_menu != 1){

		/* Execute emulator:
		 */
		if (!mainWin->IsFrozen())
			Exec6502Instruction();


		/* If the mouse cursor should be hidden (set on GUI),
		 * then make sure it is hidden after a suitable delay.
		 * (Delay is set in the menu event code below) 
		 */
		if (mainWin->CursorShouldBeHidden()
		 && SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE
		 && EG_Draw_CalcTimePassed(ticks
		 , SDL_GetTicks()) >= 2500)
			SDL_ShowCursor(SDL_DISABLE);

		while (SDL_PollEvent(&event))
			switch (event.type)
			{
			case SDL_QUIT:
				done=1;
			break;


			case SDL_MOUSEMOTION:
//			if (event.type == SDL_MOUSEMOTION){
				if (mainWin)
				{
//--                            mainWin->ScaleMousestick(LOWORD(lParam), HIWORD(lParam));
//--                            mainWin->SetAMXPosition(LOWORD(lParam), HIWORD(lParam));

				mainWin->ScaleMousestick( (unsigned int) event.motion.x
				 , (unsigned int) event.motion.y );
				mainWin->SetAMXPosition( (unsigned int) event.motion.x
				 , (unsigned int) event.motion.y );

					// Experiment: show menu in full screen when cursor moved to top of window
					//-- if (HideMenuEnabled)
					//-- {
					//-- 	if (HIWORD(lParam) <= 2)
					//-- 		mainWin->ShowMenu(true);
					//-- 	else
					//-- 		mainWin->ShowMenu(false); 
					//-- }
				}
//			}
			break;




			case SDL_MOUSEBUTTONDOWN:

//			if (event.type == SDL_MOUSEBUTTONDOWN){
			//-- case WM_LBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT){
//					printf("left button down\n");
//--					if (mainWin) mainWin->SetMousestickButton(TRUE);
					AMXButtons |= AMX_LEFT_BUTTON;
				}
			//-- case WM_MBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_MIDDLE){
//					printf("middle button down\n");
					AMXButtons |= AMX_MIDDLE_BUTTON;
				}
			//-- case WM_RBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_RIGHT){
//				 	printf("right button down\n");
					AMXButtons |= AMX_RIGHT_BUTTON;
				}
//			}

			break;


			case SDL_MOUSEBUTTONUP:

//			if (event.type == SDL_MOUSEBUTTONUP){
			//-- case WM_LBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT){
//					printf("left button up\n");
//--					if (mainWin) mainWin->SetMousestickButton(FALSE);
					AMXButtons &= ~AMX_LEFT_BUTTON;
				}
			//-- case WM_MBUTTONUP:
				if (event.button.button == SDL_BUTTON_MIDDLE){
//					printf("middle button up\n");
					AMXButtons &= ~AMX_MIDDLE_BUTTON;
				}
			//-- case WM_RBUTTONUP:
				if (event.button.button == SDL_BUTTON_RIGHT){
//					printf("right button up\n");
					AMXButtons &= ~AMX_RIGHT_BUTTON;
				}
//			}
			break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				{
					int pressed=0, col=0, row=0;

					if (event.key.keysym.sym == SDLK_F12 || event.key.keysym.sym == SDLK_F11 || event.key.keysym.sym == SDLK_MENU){
						Show_Main();
					}


					pressed = col = row = 0;

					/* Handle shift booting:
					 */
					if (mainWin->m_ShiftBooted){
						mainWin->m_ShiftBooted = false;
						BeebKeyUp(0, 0);
					}

					/* Convert SDL key press into BBC key press:
					 */
					if (ConvertSDLKeyToBBCKey(event.key.keysym /*, &pressed */, &col, &row)){
						/* If X11 and Caps Lock then release automatically after 20
						 * passes to the emulator core (doesn't X11 suck)..
						 *
						 * We'll only ever receive a pressed event for Caps Lock, so
						 * emulate a release after x amount of time passes within the
						 * emulator core.
						 *
						 * (For games we can use another key, defaults to Windows Left
						 *  key for the moment).
						 */
						if (event.key.keysym.sym == SDLK_CAPSLOCK && cfg_HaveX11){
							 BeebKeyDown(4, 0);
							 X11_CapsLock_Down = 20;
						}else{

							/* Process key in emulator core:
							 */
//							if (pressed)
							if (event.type == SDL_KEYDOWN)
								BeebKeyDown(row, col);
							else
								BeebKeyUp(row, col);
						}
		


//						/* Release Caps lock for X11 after a short delay.
//						 */
//						if (event.key.keysym.sym == SDLK_CAPSLOCK && cfg_HaveX11){
//							printf("Need to release Caps\n");
//							//SDL_Delay(1000);
//							X11_CapsLock_Down = 10;
//						}

						/* Handle reset:
						 */
						if(row==-2){ // Must do a reset!
							Init6502core();
							if (EnableTube)
								Init65C02core();

							if (Tube186Enabled) i86_main();
							Enable_Z80 = 0;
							if (TorchTube || AcornZ80)
								{
								R1Status = 0;
								ResetTube();
								init_z80();
								Enable_Z80 = 1;
							}
							Disc8271_reset();
							Reset1770();
#							ifdef WITH_ECONET
							if (EconetEnabled)
								EconetReset();//Rob
#							endif
							SCSIReset();
							SASIReset();
							TeleTextInit();
							//SoundChipReset();
						}
					}
				}
			}
		
		}else{
			/* Make sure mouse pointer is shown when menu is displayed
			 */
			if (SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
				SDL_ShowCursor(SDL_ENABLE);


			/* Process window manager signals:
			 */
			while ( SDL_PollEvent(&event) ){
				switch (event.type)
				{
				case SDL_QUIT:
				done=1;
				break;

#				ifdef WITH_DEBUG_OUTPUT
				/* For now allow exit with ESCAPE.
				 */
				case SDL_KEYDOWN:
//				if(event.key.keysym.sym == SDLK_ESCAPE)
//					done=1;
				break;
#				endif
				}

				/* Send event to GUI:
				 */
				EG_Window_ProcessEvent(displayed_window_ptr, &event, 0, 0);
			}

			//printf("Menu mode.\n");
			//SDL_Delay(10); // ARJ
			EG_Window_ProcessEvent(displayed_window_ptr, NULL, 0, 0);
			RenderTexture();
			/* Record time so we can hide the mouse cursor after a small delay.
			 */
			ticks = SDL_GetTicks();


		}

		//	printf("%d\n", AMXButtons);


//--	} while(1);
//	} while(done==0);

}

//------------------------------------------------

//-- int CALLBACK WinMain(HINSTANCE hInstance, 
//-- 					HINSTANCE hPrevInstance,
//-- 					LPSTR lpszCmdLine,
//-- 					int nCmdShow)
//-- { 

// ARJ Override browser key handling
EM_JS(void, browser_prevent_default, (), {
    document.onkeydown = function(event) {
            event.returnValue = false;
    };
  });

EM_JS(int, browser_load_settings, (), {
    window.beebem={};
    window.beebem.disc1='/usr/local/share/beebem/media/discs/blank.ssd';
    window.beebem.disc2='';
    // Parse the query string
    var urlParams={};
    var match,
        search = /([^&=]+)=?([^&]*)/g,
        decode = function (s) { return decodeURIComponent(s.replace("+", " ")); },
        query  = window.location.search.substring(1);
    while (match = search.exec(query))
       urlParams[decode(match[1])] = decode(match[2]);

    if (urlParams.disc1) {
      var filename_disc1 = urlParams.disc1.replace(/^.*[\\\/]/, '');
      window.beebem.disc1='#';
      console.log("Loading: "+urlParams.disc1+' to : '+filename_disc1);
      var request_disc1 = new XMLHttpRequest();
      // https://cors-anywhere.herokuapp.com/
      request_disc1.open('GET', 'https://cors-anywhere.webassembly.link/'+urlParams.disc1, true);
      request_disc1.responseType = "arraybuffer";
      request_disc1.overrideMimeType("application/octet-stream");
      request_disc1.onreadystatechange = function () {
	if (request_disc1.readyState === 4) {
	  if (request_disc1.status === 200) {
	    console.log("disc1 loaded");
	    window.beebem.disc1='/usr/local/share/beebem/media/discs/'+filename_disc1;
	    FS.writeFile(window.beebem.disc1, new Uint8Array(request_disc1.response), { encoding: "binary" });
	  } else {
	    window.beebem.disc1='#error';
	  }
	}
      };
      request_disc1.send();
    }
    if (urlParams.disc2) {
      var filename_disc2 = urlParams.disc2.replace(/^.*[\\\/]/, '');
      console.log("Loading: "+urlParams.disc2+' to : '+filename_disc2);
      var request_disc2 = new XMLHttpRequest();
      request_disc2.open('GET', 'https://cors-anywhere.webassembly.link/'+urlParams.disc2, true);
      request_disc2.responseType = "arraybuffer";
      request_disc2.overrideMimeType("application/octet-stream");
      request_disc2.onreadystatechange = function () {
	if (request_disc2.readyState === 4 && request_disc2.status === 200) {
	  console.log("disc2 loaded");
	  window.beebem.disc2='/usr/local/share/beebem/media/discs/'+filename_disc2;
	  FS.writeFile(window.beebem.disc2, new Uint8Array(request_disc2.response), { encoding: "binary" });
	}
      };
      request_disc2.send();
    }
    if (urlParams.bbcromd) {
      var filename_bbcromd = urlParams.bbcromd.replace(/^.*[\\\/]/, '');
      console.log("Loading: "+urlParams.bbcromd+' to : roms/'+filename_bbcromd);
      var request_bbcromd = new XMLHttpRequest();
      request_bbcromd.open('GET', 'https://cors-anywhere.webassembly.link/'+urlParams.bbcromd, true);
      request_bbcromd.responseType = "arraybuffer";
      request_bbcromd.overrideMimeType("application/octet-stream");
      request_bbcromd.onreadystatechange = function () {
	if (request_bbcromd.readyState === 4 && request_bbcromd.status === 200) {
	  console.log("Rom Loaded");
	  FS.writeFile("/usr/local/share/beebem/roms/bbc/romd.rom", new Uint8Array(request_bbcromd.response), { encoding: "binary" });
	}
      };
      request_bbcromd.send();
    }
    if (urlParams.bbcromc) {
      var filename_bbcromc = urlParams.bbcromc.replace(/^.*[\\\/]/, '');
      console.log("Loading: "+urlParams.bbcromc+' to : roms/'+filename_bbcromc);
      var request_bbcromc = new XMLHttpRequest();
      request_bbcromc.open('GET', 'https://cors-anywhere.webassembly.link/'+urlParams.bbcromc, true);
      request_bbcromc.responseType = "arraybuffer";
      request_bbcromc.overrideMimeType("application/octet-stream");
      request_bbcromc.onreadystatechange = function () {
	if (request_bbcromc.readyState === 4 && request_bbcromc.status === 200) {
	  console.log("Rom Loaded");
	  FS.writeFile("/usr/local/share/beebem/roms/bbc/romc.rom", new Uint8Array(request_bbcromc.response), { encoding: "binary" });
	}
      };
      request_bbcromc.send();
    }
    if (urlParams.bbcromd && urlParams.bbcromc) {return 0x12;}
    if (urlParams.bbcromd) {return 0x10;}
    if (urlParams.bbcromc) {return 0x11;}

    if (urlParams.autoboot) {
      console.log('autoboot');
      window.beebem.autoboot=1;
      return 1;
    }

    // auto-load the disc
    if (urlParams.disc1) {
      return 2;
    }

    
    return 3;
  });

EM_JS(const char*, browser_check_disc, (), {
    //if (window.beebem.disc1=='#error') return -1;
    //return window.beebem.disc1.length;
    var lengthBytes = lengthBytesUTF8(window.beebem.disc1)+1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(window.beebem.disc1, stringOnWasmHeap, lengthBytes+1);
    return stringOnWasmHeap;
});



void multi_iter() {
  if (autoboot!=0) {
    if (autoboot==1) {
      if (fopen(browser_check_disc(), "rb") != NULL) {
	mainWin->HandleCommand(IDM_RUNDISC);
	autoboot=0;
      }
    }
    else if (autoboot==2) {
      if (fopen(browser_check_disc(), "rb") != NULL) {
	mainWin->HandleCommand(IDM_LOADDISC0);
	autoboot=0;
      }
    }
    else if (autoboot==3) {
      // load in blank disk
      autoboot=0;
      mainWin->HandleCommand(IDM_LOADDISC0);
      mainWin->HandleCommand(IDM_WPDISC0);
    } else if (autoboot==0x10) {
      // Check for romd
      if (fopen("/usr/local/share/beebem/roms/bbc/romd.rom","rb") !=NULL) {
	mainWin->HandleCommand(IDM_LOADDISC0);
	mainWin->HandleCommand(IDM_WPDISC0);
	mainWin->HandleCommand(ID_FILE_RESET);
	autoboot = 0;
      }
    } else if (autoboot==0x11) {
      // Check for romc
      if (fopen("/usr/local/share/beebem/roms/bbc/romc.rom","rb") !=NULL) {
	mainWin->HandleCommand(IDM_LOADDISC0);
	mainWin->HandleCommand(IDM_WPDISC0);
	mainWin->HandleCommand(ID_FILE_RESET);
	autoboot = 0;
      }
    } else if (autoboot==0x12) {
      // Check for romd and romc
      if (fopen("/usr/local/share/beebem/roms/bbc/romd.rom","rb") !=NULL) {
	  if (fopen("/usr/local/share/beebem/roms/bbc/romc.rom","rb") !=NULL) {
	    mainWin->HandleCommand(IDM_LOADDISC0);
	    mainWin->HandleCommand(IDM_WPDISC0);
	    mainWin->HandleCommand(ID_FILE_RESET);
	    autoboot = 0;
	  }
      }
    }
  }
  for (int l=0; l<20; l++) {
    one_iter();
  }
}


int main(int argc, char *argv[]){
//--	MSG msg;

	//printf("%d\n", testingit(1, 2, 3));



//	print_message();
//	SDL_Delay(5000);

/*
#define STRING_HASH "EG_Widget_Type_ToggleButton"
	printf(STRING_HASH " %lX\n", EG_MakeStringHash(STRING_HASH) );
	exit(1);
*/

//+>
//	int X11_CapsLock_Down;
//	Uint32 ticks=SDL_GetTicks();

  // ARJ
  ticks=SDL_GetTicks();
  
//	int mouse_x=0, mouse_y=0, mouse_move_x, mouse_move_y;
//	BOOL ignore_next_mouse_movement = FALSE;
//	int buttons=0;
//<+

//--	hInst = hInstance;

	/* Create global reference to command line args (like windows does)
	 */
	__argc = argc;
	__argv = (char**) argv;

	printf("BeebEm for WebAssembly - https://beeb.webassembly.link\n"); 
	printf("PC: press shift-del to boot\nMac : press fn-shift-backspace to boot\n"); 
	browser_prevent_default();
	autoboot = browser_load_settings();

	/* Initialise debugging subsystem.
	 */
	Log_Init();

	/* Initialize SDL resources.
	 */
	if (! InitialiseSDL(argc, argv)){
		qFATAL("Unable to initialise SDL library!");
		exit(1);
	}

	qINFO("ARJ.");
/* Initialize GUI API
 */
	if (EG_Initialize() == EG_TRUE){
		qINFO("EG initialized.");
	}else{
		qFATAL("EG failed to initialize! Quiting.");
				exit(1);
	}


	/* Build menus:
	*/
	if (InitializeBeebEmGUI(screen_ptr) != EG_TRUE)
		exit(1);

	/* Initialize fake windows registry:
	 */
	InitializeFakeRegistry();

	/* ---------------------------------------
	 */

	// Create instance of Emulator core:
	mainWin=new BeebWin();
	mainWin->Initialise();

	/* ------------------------------------------------------
	 */

	// Create serial threads
//--	InitThreads();
//--	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) SerialThread,NULL,0,&iSerialThread);
//--	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) StatThread,NULL,0,&iStatThread);

	/* -------------------------------------------------0----------
	 */

//    tlog = fopen("\\trace.log", "wt");


	/* Clear SDL event queue
	 */
	EG_Draw_FlushEventQueue();

	X11_CapsLock_Down = 0;	// =0 not down, used to emulate a key release in X11 (caps has no release event).

	done = 0;
#ifdef __EMSCRIPTEN__
       	//EM_ASM("SDL.defaults.copyOnLock = true; SDL.defaults.discardOnLock = false; SDL.defaults.opaqueFrontBuffer = true");
	//EM_ASM("SDL.defaults.copyOnLock = false");
	//emscripten_set_main_loop(multi_iter, 2000, 1);
	emscripten_set_main_loop(multi_iter, 0, 1);
#else
	do {
	  one_iter();
	} while(done==0);
#endif





/* THIS WILL EVENTUALLY MOVE INTO beebwin.cpp when the MFC event loop is faked
 * (until then the rest of this file will be a mess):
 * -------------------------------------------
 */

//	pDEBUG(dL"Got to main loop!", dR);

//--	mainWin->KillDLLs();

//    	fclose(tlog);

	/* Sometimes the mouse pointer vanishes on exit.
	 * It seems to only happen when fullscreened, so make sure
	 * we unfullscreen before destroying everything.
	 */
	UnfullscreenBeforeExit();

	delete mainWin;
//--	Kill_Serial();

	/* Cleanly free SDL and logging.
	 */
#ifdef WITH_DEBUG_OUTPUT
	DumpFakeRegistry();
#endif

	DestroyFakeRegistry();
	DestroyBeebEmGUI();
	UninitialiseSDL();
	Log_UnInit();

	return(0);  
} /* main */
