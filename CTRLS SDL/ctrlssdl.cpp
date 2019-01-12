/* joysdl.c - this file is part of DeSmuME
 *
 * Copyright (C) 2007 Pascal Giard
 *
 * Author: Pascal Giard <evilynux@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

//HCF 9
#include <string.h>

#include "ctrlssdl.h"
#ifdef __psp__
	#include <pspctrl.h>
    #include "./psp/FrontEnd.h"
#endif

//XBOX: MENU
#define MOV_ARRIBA 0
#define MOV_ABAJO 1
#define MOV_AA 2
#define MOV_BB 3
//#define MENU_FIRST_OPTION_Y  150
//////////////#define MENU_FIRST_OPTION_Y   40
//HCF Added 3d and dynarec options
#define MENU_FIRST_OPTION_Y   35
#define MENU_OPTION_HEIGHT    20
#define MENU_RECTANGLE_OFFSET_Y 1
#define MENU_RECTANGLE_OFFSET_X 5

//HCF SDL CONTROLS
//gamepad stuff
Sint16 joyx, joyy; //axes
Sint16 blackbutton;
Sint16 whitebutton;
Sint16 abutton; //A button
Sint16 bbutton;
Sint16 xbutton;
Sint16 ybutton;
Sint16 backbutton; //BACK button
Sint16 startbutton; //START button
Sint16 rstick; 
Sint16 dpad; //dpad
Sint16 ltrigger, rtrigger; //Trigger buttons
SDL_Joystick *GAMEPAD; //Gamepad
short ashBotones[NUM_BOTONES];

TTF_Font *font;
SDL_Surface *SDLscreen;


//HCF Stretch:
int iModoStretch      = STRETCH_MODE_NONE;
int iModoStretchNuevo = STRETCH_MODE_NONE;
int iCambiandoModoStretch = 0;

//HCF 0-doble screen, 1 solo arriba, 2 solo abajo
int iModoGrafico = 0;
int iModoGraficoNuevo = 0;
int iCambiandoModoGrafico = 0;
int iMouseSpeed = MOUSE_DEFAULT_SPEED;
bool bAutoFrameskip;
int nFrameskip;  //This will store the frameskip configured by the user 
    		     //On the other hand, "frameskip" stores the actual frameskip
int iUsarDynarec = 1; //iGlobalSpeed = 1;

BOOL bBlitAll = true;
int emula3D = 1;

//HCF: Sound overclocking
int iEnableSound;

//HCF 9
struct mouse_status mouse;
/* Current keyboard configuration */
u16 keyboard_cfg[NB_KEYS];
/* Current joypad configuration */
u16 joypad_cfg[NB_KEYS];
/* Number of detected joypads */
u16 nbr_joy;

static SDL_Joystick **open_joysticks = NULL;

// Vertical
#ifdef __psp__
const u16 default_psp_cfg_v[NB_KEYS] =
  { PSP_CTRL_CIRCLE,    //A
	PSP_CTRL_CROSS,     //B
	PSP_CTRL_SELECT,	//Select
	PSP_CTRL_START,		//Start
	PSP_CTRL_DOWN,		//Right
	PSP_CTRL_UP,		//Left
	PSP_CTRL_RIGHT,		//Up
	PSP_CTRL_LEFT,		//Down
	PSP_CTRL_RTRIGGER,	//R
	PSP_CTRL_LTRIGGER,	//L
	PSP_CTRL_TRIANGLE,  //X
	PSP_CTRL_SQUARE     //Y
  };
#endif
  //HCF
const u16 default_xb_cfg_v[NB_KEYS] =
  { BOTON_AA,    //A
	BOTON_BB,     //B
	BOTON_BACK,	//Select
	BOTON_START,		//Start
	BOTON_ABAJO,		//Right
	BOTON_ARRIBA,		//Left
	BOTON_DERECHA,		//Up
	BOTON_IZQUIERDA,		//Down
	BOTON_RTRIGGER,	//R
	BOTON_LTRIGGER,	//L
	BOTON_XX,  //X
	BOTON_YY     //Y
  };  

// Horizontal
#ifdef __psp__
const u16 default_psp_cfg_h[NB_KEYS] =
  { PSP_CTRL_CIRCLE,    //A
	PSP_CTRL_CROSS,     //B
	PSP_CTRL_SELECT,	//Select
	PSP_CTRL_START,		//Start
	PSP_CTRL_RIGHT,		//Right
	PSP_CTRL_LEFT,		//Left
	PSP_CTRL_UP,		//Up
	PSP_CTRL_DOWN,		//Down
	PSP_CTRL_RTRIGGER,	//R
	PSP_CTRL_LTRIGGER,	//L
	PSP_CTRL_TRIANGLE,  //X
	PSP_CTRL_SQUARE     //Y
  };
#endif
//HCF
const u16 default_xb_cfg_h[NB_KEYS] =
  { BOTON_AA,    //A
	BOTON_BB,     //B
	BOTON_BACK,	//Select
	BOTON_START,		//Start
	BOTON_DERECHA,		//Right
	BOTON_IZQUIERDA,		//Left
	BOTON_ARRIBA,		//Up
	BOTON_ABAJO,		//Down
	BOTON_RTRIGGER,	//R
	BOTON_LTRIGGER,	//L
	BOTON_XX,  //X
	BOTON_YY     //Y
  };

/* Keypad key names */
const char *key_names[NB_KEYS] =
{
  "A", "B", "Select", "Start",
  "Right", "Left", "Up", "Down",
  "R", "L", "X", "Y",
  "Debug", "Boost"
};

/* Default joypad configuration */
const u16 default_joypad_cfg[NB_KEYS] =
  { 1,  // A
    0,  // B
    5,  // select
    8,  // start
    256, // Right -- Start cheating abit...
    256, // Left
    512, // Up
    512, // Down  -- End of cheating.
    7,  // R
    6,  // L
    4,  // X
    3,  // Y
    -1, // DEBUG
    -1  // BOOST
  };

const u16 default_keyboard_cfg[NB_KEYS] =
{
  97, // a
  98, // b
  65288, // backspace
  65293, // enter
  65363, // directional arrows
  65361,
  65362,
  65364,
  65454, // numeric .
  65456, // numeric 0
  120, // x
  121, // y
  112,
  113
};

//HCF QUITO FONT
void vdRellenaBotones(void)
{
	SDL_PumpEvents();
	//keys = SDL_GetKeyState(NULL);
	SDL_JoystickUpdate(); //manual refresh of the gamepad(s)
	//parse all gamepads
	
	joyx = SDL_JoystickGetAxis(GAMEPAD, 0);
	joyy = SDL_JoystickGetAxis(GAMEPAD, 1);
	dpad = SDL_JoystickGetHat(GAMEPAD, 0);
	blackbutton = SDL_JoystickGetButton(GAMEPAD, 4); //Black Button
	whitebutton = SDL_JoystickGetButton(GAMEPAD, 5); //White Button, comprobarlo!!
	abutton = SDL_JoystickGetButton(GAMEPAD, 0); //Get A-Button(0)
	bbutton = SDL_JoystickGetButton(GAMEPAD, 1);
	backbutton = SDL_JoystickGetButton(GAMEPAD, 9); //Get BACK-Button(9)
	rstick = SDL_JoystickGetButton(GAMEPAD,11);
	ltrigger = SDL_JoystickGetButton(GAMEPAD,6);
	rtrigger = SDL_JoystickGetButton(GAMEPAD,7);
	xbutton = SDL_JoystickGetButton(GAMEPAD,2);
	ybutton = SDL_JoystickGetButton(GAMEPAD,3);
	startbutton = SDL_JoystickGetButton(GAMEPAD,8);
	

	if(startbutton)
	{
	    ashBotones[BOTON_START] = 1;
	}
	else
	{
	    ashBotones[BOTON_START] = 0;
	}
	if(ltrigger)
	{
	    ashBotones[BOTON_LTRIGGER] = 1;
	}
	else
	{
	    ashBotones[BOTON_LTRIGGER] = 0;
	}
	if(rtrigger)
	{
	    ashBotones[BOTON_RTRIGGER] = 1;
	}
	else
	{
	    ashBotones[BOTON_RTRIGGER] = 0;
	}
	if(rstick)
	{
	    ashBotones[BOTON_RTHUMBSTICK] = 1;
	}
	else
	{
	    ashBotones[BOTON_RTHUMBSTICK] = 0;
	}
	if(backbutton)
	{
	    ashBotones[BOTON_BACK] = 1;
	}
	else
	{
	    ashBotones[BOTON_BACK] = 0;
	}
	if(abutton)
	{
	    ashBotones[BOTON_AA] = 1;
	}
	else
	{
	    ashBotones[BOTON_AA] = 0;
	}
	if(blackbutton)
	{
	    ashBotones[BOTON_NEGRO] = 1;
	}
	else
	{
	    ashBotones[BOTON_NEGRO] = 0;
	}
	if(whitebutton)
	{
	    ashBotones[BOTON_BLANCO] = 1;
	}
	else
	{
	    ashBotones[BOTON_BLANCO] = 0;
	}
	if(bbutton)
	{
	    ashBotones[BOTON_BB] = 1;
	}
	else
	{
	    ashBotones[BOTON_BB] = 0;
	}
	if(ybutton)
	{
	    ashBotones[BOTON_YY] = 1;
	}
	else
	{
	    ashBotones[BOTON_YY] = 0;
	}
	if(xbutton)
	{
	    ashBotones[BOTON_XX] = 1;
	}
	else
	{
	    ashBotones[BOTON_XX] = 0;
	}
	
	if(dpad & SDL_HAT_UP)
	{
		ashBotones[BOTON_ARRIBA] = 1;
	}
	else
	{
	    ashBotones[BOTON_ARRIBA] = 0;
	}
	if(dpad & SDL_HAT_DOWN)
	{
		ashBotones[BOTON_ABAJO] = 1;
	}
	else
	{
	    ashBotones[BOTON_ABAJO] = 0;
	}
	if(dpad & SDL_HAT_LEFT)
	{
		ashBotones[BOTON_IZQUIERDA] = 1;
	}
	else
	{
	    ashBotones[BOTON_IZQUIERDA] = 0;
	}
	if(dpad & SDL_HAT_RIGHT)
	{
		ashBotones[BOTON_DERECHA] = 1;
	}
	else
	{
	    ashBotones[BOTON_DERECHA] = 0;
	}
	
}

void vdXBOptionsMenu(int iMenuInicial)
{
	char cAux = '0';
		
	int iRedibujar = 1;
	int iLongitud;
	int salir = 0;
	int movanterior = MOV_AA;
	int iNumOpciones;
	int iOpcionSeleccionada = 0;

	int iVideoModeChanged = 0;
	
	SDL_Surface *picTexto;
	SDL_Surface *picRectangle;
	SDL_Surface *picRectangleTemp;
	SDL_Rect rectVentana;
	SDL_Rect rectTexto;
	
	char achTexto[100];
	
	SDL_Color textColor = { 255, 255, 255 }; 
	
	//HCF Por que se guardaba en temporal?
	///iFrameskip = nFrameskip;

	picRectangleTemp = IMG_Load("rectangle.png");	
			
	SDL_SetColorKey( picRectangleTemp, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(picRectangleTemp->format, 255, 0, 255) );
	picRectangle = SDL_DisplayFormatAlpha(picRectangleTemp);
	SDL_FreeSurface(picRectangleTemp);
	
	//rectVentana.x = 100;
	//rectVentana.x = (SDLscreen->w - 300) / 2;
	rectVentana.x = (SDLscreen->w - 250) / 2;
	//rectVentana.y = 100;
	rectVentana.y = 0;
	//rectVentana.w = 400;
	//rectVentana.w = 300;
	rectVentana.w = 250;


	//rectVentana.h = 300;
	//rectVentana.h = 250;
	//rectVentana.h = 280;
	rectVentana.h = 200;
	//desmumeX

	//rectTexto.x = 130;
	//rectTexto.x = 30;
	rectTexto.x = rectVentana.x + 30;
	//rectTexto.y = 130;
	rectTexto.y = 30;
	
	if( iMenuInicial == 1 )
	{
		//iNumOpciones = 4;	
		//HCF desmumeX!!
		
		//iNumOpciones = 6;	
		//Se anyade bAutoFrameskip

		iNumOpciones = 8;	
		//Se anyaden emula3D y bUsaDynarec
	}
	else
	{
		//iNumOpciones = 1;	

		//En desmumeX mostramos todas las opciones
		iNumOpciones = 8;
	}

	while(!salir)
	{
	
		//g_input.Update();
		vdRellenaBotones();
		//if(g_input.IsButtonPressed(Generic_DPadUp))
		if( ashBotones[BOTON_ARRIBA] )
		{
			if( movanterior != MOV_ARRIBA )
			{
				if( iOpcionSeleccionada > 0 )
				{
					iOpcionSeleccionada--;
					iRedibujar = 1;
				}
				
			}
			movanterior = MOV_ARRIBA;
		}
		else if(movanterior == MOV_ARRIBA)
		{
			movanterior = -1;
		}
		
		//if(g_input.IsButtonPressed(Generic_DPadDown))
		if( ashBotones[BOTON_ABAJO] )
		{
			if( movanterior != MOV_ABAJO )
			{
				if( iOpcionSeleccionada < iNumOpciones - 1 )
				{
					iOpcionSeleccionada++;
					iRedibujar = 1;
				}
			}
			movanterior = MOV_ABAJO;
		}
		else if(movanterior == MOV_ABAJO)
		{
			movanterior = -1;
		}	
		
		//if(g_input.IsButtonPressed(Generic_A))
		if( ashBotones[BOTON_AA] )
		{
			if( movanterior != MOV_AA )
			{
				if( iOpcionSeleccionada == 3 )
				{
					//SOUND
					//Asi era para solo ON y OFF
					/////iEnableSound = ( ( iEnableSound + 1 ) % 2 );
					//HCF Add audio overclocking
					iEnableSound = ( ( iEnableSound + 1 ) % 5 );
					enable_sound = iEnableSound;
					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 4 )
				{
					//MOUSE Speed
					if(iMouseSpeed < 9)
						iMouseSpeed++;
					
					//else
					//	iSpeedMouse = 1;	
					
										
					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 5 )
				{
				    //Se sustituye global speed por blit all
					//BLIT ALL
					if(bBlitAll == true)
					{
						bBlitAll = false;
					}
					else
					{
						bBlitAll = true;
					}
					//Global Speed
					/*
					if(iGlobalSpeed < 8)
						iGlobalSpeed++;
					*/
					//else
					//	iGlobalSpeed = 1;	
					
										
					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 6 )
				{
					switch(iSoundQuality)
					{
						case 8:				
							iSoundQuality = 4;
							break;
						case 4:				
							iSoundQuality = 2;
							break;
						case 2:				
							iSoundQuality = 1;
							break;
						default:
							break;
					}
									
					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 2 )
				{
					//FS
					if( nFrameskip < 9 )
					{
						nFrameskip++;
						iRedibujar = 1;
					}

					frameskip = nFrameskip;
				}
				else if( iOpcionSeleccionada == 1 )
				{
					//Auto Framekip
					if(bAutoFrameskip == true)
					{
						bAutoFrameskip = false;
					}
					else
					{
						bAutoFrameskip = true;
					}

					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 0 )
				{
					//3D
					if(emula3D == true)
					{
						emula3D = false;
					}
					else
					{
						emula3D = true;
					}

					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 7 )
				{
					//Use Dynarec
					if(iUsarDynarec == true)
					{
						iUsarDynarec = false;
					}
					else
					{
						iUsarDynarec = true;
					}

					iRedibujar = 1;
				}
				
			}
			movanterior = MOV_AA;
		}
		else if(movanterior == MOV_AA)
		{
			movanterior = -1;
		}

		//if(g_input.IsButtonPressed(Generic_B))
		if( ashBotones[BOTON_BB] )
		{
			if( movanterior != MOV_BB )
			{
				if( iOpcionSeleccionada == 3 )
				{
					//SOUND
					//iEnableSound = ( ( iEnableSound + 1 ) % 2 );
					if(iEnableSound > 0)
						iEnableSound--;
					else
						iEnableSound = 4;
						//HCF Asi era para solo ON/OFF
						//iEnableSound = 1;

					enable_sound = iEnableSound;
					iRedibujar = 1;
					
				}
				else if( iOpcionSeleccionada == 4 )
				{
					//MOUSE SPEED
					if(iMouseSpeed > 1)
						iMouseSpeed--;
					
					//else
					//	iSpeedMouse = 9;
					
					
					iRedibujar = 1;
					
				}
				else if( iOpcionSeleccionada == 5 )
				{
				    //Se sustituye global speed por blit all
					//BLIT ALL
					if(bBlitAll == true)
					{
						bBlitAll = false;
					}
					else
					{
						bBlitAll = true;
					}
					//GLOBAL SPEED
					/*
					if(iGlobalSpeed > 1)
						iGlobalSpeed--;
					*/				
					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 6 )
				{
					switch(iSoundQuality)
					{
						case 1:				
							iSoundQuality = 2;
							break;
						case 2:				
							iSoundQuality = 4;
							break;
						case 4:				
							iSoundQuality = 8;
							break;
						default:
							break;
					}
									
					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 2 )
				{
					//FS
					if( nFrameskip > 0 )
					{
						nFrameskip--;
						iRedibujar = 1;

						frameskip = nFrameskip;
					}
				}
				else if( iOpcionSeleccionada == 1 )
				{
					//Auto Framekip
					if(bAutoFrameskip == true)
					{
						bAutoFrameskip = false;
					}
					else
					{
						bAutoFrameskip = true;
					}

					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 0 )
				{
					//3D
					if(emula3D == true)
					{
						emula3D = false;
					}
					else
					{
						emula3D = true;
					}

					iRedibujar = 1;
				}
				else if( iOpcionSeleccionada == 7 )
				{
					//Use Dynarec
					if(iUsarDynarec == true)
					{
						iUsarDynarec = false;
					}
					else
					{
						iUsarDynarec = true;
					}

					iRedibujar = 1;
				}
				
			}
			movanterior = MOV_BB;
		}
		else if(movanterior == MOV_BB)
		{
			movanterior = -1;
		}

		//if(g_input.IsButtonPressed(Generic_Start))
		if( ashBotones[BOTON_START] )
		{
			salir = 1;
		}
			
		if( iRedibujar)
		{
			//rectVentana.x = 100;
			//rectVentana.x = (SDLscreen->w - 300) / 2;
			rectVentana.x = (SDLscreen->w - 250) / 2;
			//rectVentana.y = 100;
			rectVentana.y = 0;

			//HCF Adding options...
			//rectVentana.h = 300;
			//rectVentana.h = 280;
			rectVentana.h = 200;
			//desmumeX
			
			SDL_FillRect(SDLscreen, &rectVentana, SDL_MapRGB(SDLscreen->format, 0, 50, 50));

			//TEXTO OPTIONS
			//strcpy(achTexto, "OPTIONS");
			
			//TEXTO 3D Emulation (sustituye al texto de OPTIONS)
			if(emula3D)
			{
				strcpy(achTexto, "3D emulation: ON");
			}
			else
			{
				strcpy(achTexto, "3D emulation: OFF");
			}
						
			/////rectTexto.y = 17;
			rectTexto.y = MENU_FIRST_OPTION_Y - MENU_OPTION_HEIGHT;
			picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
			SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
			SDL_FreeSurface(picTexto);
		
			//TEXTO AUTO FRAMESKIP
			if(bAutoFrameskip)
			{
				strcpy(achTexto, "Frameskip: Auto");
			}
			else
			{
				strcpy(achTexto, "Frameskip: Fixed");
			}
						
			rectTexto.y = MENU_FIRST_OPTION_Y;
			picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
			SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
			SDL_FreeSurface(picTexto);

			//TEXTO FRAMESKIP
			if(bAutoFrameskip)
			{
				strcpy(achTexto, "Frameskip: <=");
			}
			else
			{
				strcpy(achTexto, "Frameskip: ");
			}
			cAux = '0' + nFrameskip;
			iLongitud = strlen(achTexto);
			achTexto[iLongitud] = cAux;
			achTexto[iLongitud + 1] = '\0';
			
			rectTexto.y = MENU_FIRST_OPTION_Y + MENU_OPTION_HEIGHT;
			picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
			SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
			SDL_FreeSurface(picTexto);
			
			//TEXTO SOUND, solo en menu inicial
			//if( iMenuInicial == 1 )
			//{
				if(iEnableSound == 0)
				{
					strcpy(achTexto, "Sound OFF");
				}
				else if(iEnableSound == 1)
				{
					strcpy(achTexto, "Sound On");
				}
				////
				else if(iEnableSound == 2)
				{
					strcpy(achTexto, "Sound On, Overclocked x2");
				}
				else if(iEnableSound == 3)
				{
					strcpy(achTexto, "Sound On, Overclocked x3");
				}
				else if(iEnableSound == 4)
				{
					strcpy(achTexto, "Sound On, Overclocked x4");
				}
				//////

				rectTexto.y = ( (2 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
				picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
				SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
				SDL_FreeSurface(picTexto);

				//TEXTO MOUSE
				sprintf(achTexto, "Pointer Speed: %d", iMouseSpeed);
				rectTexto.y = ( (3 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
				picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
				SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
				SDL_FreeSurface(picTexto);

				//TEXTO BUS SPEED	
				//SE CAMBIA POR BLIT ALL
				if(bBlitAll)
				{
					strcpy(achTexto, "Graphics: everything");
				}
				else
				{
					strcpy(achTexto, "Graphics: optimized");
				}				
				//sprintf(achTexto, "Bus Speed: x%d", iGlobalSpeed);
				rectTexto.y = ( (4 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
				picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
				SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
				SDL_FreeSurface(picTexto);

				//TEXTO SOUND QUALITY
				if(iSoundQuality == 1)
				{
					strcpy(achTexto, "Audio Channels: 16");
				}
				else if(iSoundQuality == 2)
				{
					strcpy(achTexto, "Audio Channels: 8");
				}
				/////
				else if(iSoundQuality == 4)
				{
					strcpy(achTexto, "Audio Channels: 4");
				}
				else if(iSoundQuality == 8)
				{
					strcpy(achTexto, "Audio Channels: 2");
				}
				
				rectTexto.y = ( (5 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
				picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
				SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
				SDL_FreeSurface(picTexto);
				
				//TEXTO USE DYNAREC
				if(iUsarDynarec)
				{
					strcpy(achTexto, "Use Dynarec: Yes (faster)");
				}
				else
				{
					strcpy(achTexto, "Use Dynarec: No (slower)");
				}
							
				rectTexto.y = ( (6 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
				picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
				SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
				SDL_FreeSurface(picTexto);
				
			//} //Menu inicial

		
			
			//TEXTO INSTRUCTIONS
			strcpy(achTexto, "A/B: Toggle   START: Confirm ");
			//DesmumeX
			//rectTexto.y = 330;
			//Anyado Sound Quality

			rectTexto.y = 175;
			//rectTexto.y = 360;
			//Anyado Auto Frameskip

			picTexto = TTF_RenderText_Solid( font, achTexto, textColor ); 
			SDL_BlitSurface(picTexto, NULL, SDLscreen, &rectTexto);
			SDL_FreeSurface(picTexto);
			
			//RECTANGLE
			switch(iOpcionSeleccionada)
			{
				case 0:
					rectTexto.y = MENU_FIRST_OPTION_Y - MENU_OPTION_HEIGHT;
					break;
				case 1:
					rectTexto.y = MENU_FIRST_OPTION_Y;
					break;
				case 2:
					rectTexto.y = MENU_FIRST_OPTION_Y + MENU_OPTION_HEIGHT;
					break;
				case 3:
					rectTexto.y = ( (2 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
					break;
				case 4:
					rectTexto.y = ( (3 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
					break;
				case 5:
					rectTexto.y = ( (4 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
					break;
				case 6:
					rectTexto.y = ( (5 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
					break;
				case 7:
					rectTexto.y = ( (6 * MENU_OPTION_HEIGHT) + MENU_FIRST_OPTION_Y );
					break;
				default:
					rectTexto.y = MENU_FIRST_OPTION_Y - MENU_OPTION_HEIGHT;
					break;	
			}
			
			rectTexto.y -= MENU_RECTANGLE_OFFSET_Y;
			rectTexto.x -= MENU_RECTANGLE_OFFSET_X;
			SDL_BlitSurface(picRectangle, NULL, SDLscreen, &rectTexto);
			rectTexto.x += MENU_RECTANGLE_OFFSET_X;
						
			SDL_Flip(SDLscreen);	
		}
		
		iRedibujar = 0;
		
		SDL_Delay(20);
		
	}

	//HCF: Free the memory of the rectangle
	SDL_FreeSurface(picRectangle);
	
}

/* Load default joystick and keyboard configurations */
void load_default_config( void)
{
  memcpy(keyboard_cfg, default_keyboard_cfg, sizeof(keyboard_cfg));
  memcpy(joypad_cfg, default_joypad_cfg, sizeof(joypad_cfg));
}

/* Initialize joysticks */
BOOL init_joy( void) {
  int i;
  BOOL joy_init_good = TRUE;
  
  set_joy_keys(default_joypad_cfg);

  if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) == -1)
    {
      fprintf(stderr, "Error trying to initialize joystick support: %s\n",
              SDL_GetError());
      return FALSE;
    }

  nbr_joy = SDL_NumJoysticks();
  //printf("Nbr of joysticks: %d\n\n", nbr_joy);

  if ( nbr_joy > 0) {
    open_joysticks = (SDL_Joystick**)
      calloc( sizeof ( SDL_Joystick *), nbr_joy);

    if ( open_joysticks != NULL) {
      for (i = 0; i < nbr_joy; i++)
        {
          SDL_Joystick * joy = SDL_JoystickOpen(i);
          /*
		  printf("Joystick %d %s\n", i, SDL_JoystickName(i));
          printf("Axes: %d\n", SDL_JoystickNumAxes(joy));
          printf("Buttons: %d\n", SDL_JoystickNumButtons(joy));
          printf("Trackballs: %d\n", SDL_JoystickNumBalls(joy));
          printf("Hats: %d\n\n", SDL_JoystickNumHats(joy));
		  */
        }
    }
    else {
      joy_init_good = FALSE;
    }
  }

  return joy_init_good;
}

/* Set all buttons at once */
void set_joy_keys(const u16 joyCfg[])
{
  memcpy(joypad_cfg, joyCfg, sizeof(joypad_cfg));
}

/* Set all buttons at once */
void set_kb_keys(u16 kbCfg[])
{
  memcpy(keyboard_cfg, kbCfg, sizeof(keyboard_cfg));
}

/* Unload joysticks */
void uninit_joy( void)
{
  int i;
  //printf("Disabling joystick support.\n");

  if ( open_joysticks != NULL) {
    for (i = 0; i < SDL_NumJoysticks(); i++) {
      SDL_JoystickClose( open_joysticks[i]);
    }

    free( open_joysticks);
  }

  open_joysticks = NULL;
  SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

/* Return keypad vector with given key set to 1 */
u16 lookup_joy_key (u16 keyval) {
  int i;
  u16 Key = 0;
  for(i = 0; i < NB_KEYS; i++)
    if(keyval == joypad_cfg[i]) break;
  if(i < NB_KEYS) Key = KEYMASK_(i);
  return Key;
}

/* Return keypad vector with given key set to 1 */
u16 lookup_key (u16 keyval) {
  int i;
  u16 Key = 0;
  for(i = 0; i < NB_KEYS; i++)
    if(keyval == keyboard_cfg[i]) break;
  if(i < NB_KEYS) Key = KEYMASK_(i);
  return Key;
}

/* Empty SDL Events' queue */
static void clear_events( void)
{
  SDL_Event event;
  /* IMPORTANT: Reenable joystick events iif needed. */
  if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
    SDL_JoystickEventState(SDL_ENABLE);

  /* There's an event waiting to be processed? */
  while (SDL_PollEvent(&event))
    {
    }

  return;
}

/* Get and set a new joystick key */
u16 get_set_joy_key(int index) {
  BOOL done = FALSE;
  SDL_Event event;
  u16 key = joypad_cfg[index];

  /* Enable joystick events if needed */
  if( SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE )
    SDL_JoystickEventState(SDL_ENABLE);

  while(SDL_WaitEvent(&event) && !done)
    {
      switch(event.type)
        {
        case SDL_JOYBUTTONDOWN:
          printf( "Got joykey: %d\n", event.jbutton.button );
          key = event.jbutton.button;
          done = TRUE;
          break;
        }
    }

  if( SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE )
    SDL_JoystickEventState(SDL_IGNORE);
  joypad_cfg[index] = key;

  return key;
}

/* Reset corresponding key and its twin axis key */
u16 get_joy_axis_twin(u16 key)
{
  switch(key)
    {
    case KEYMASK_( KEY_RIGHT-1 ):
      return KEYMASK_( KEY_LEFT-1 );
    case KEYMASK_( KEY_UP-1 ):
      return KEYMASK_( KEY_DOWN-1 );
    default:
      return 0;
    }
}

/* Get and set a new joystick axis */
void get_set_joy_axis(int index, int index_o) {
  BOOL done = FALSE;
  SDL_Event event;
  u16 key = joypad_cfg[index];

  /* Clear events */
  clear_events();
  /* Enable joystick events if needed */
  if( SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE )
    SDL_JoystickEventState(SDL_ENABLE);

  while(SDL_WaitEvent(&event) && !done)
    {
      switch(event.type)
        {
        case SDL_JOYAXISMOTION:
          /* Discriminate small movements */
          if( (event.jaxis.value >> 5) != 0 )
            {
              key = JOY_AXIS_(event.jaxis.axis);
              done = TRUE;
            }
          break;
        }
    }
  if( SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE )
    SDL_JoystickEventState(SDL_IGNORE);
  /* Update configuration */
  joypad_cfg[index]   = key;
  joypad_cfg[index_o] = joypad_cfg[index];
}

static signed long
screen_to_touch_range_x( signed long scr_x, float size_ratio) {
  signed long touch_x = (signed long)((float)scr_x * size_ratio);

  return touch_x;
}

static signed long
screen_to_touch_range_y( signed long scr_y, float size_ratio) {
  signed long touch_y = (signed long)((float)scr_y * size_ratio);

  return touch_y;
}

/* Set mouse coordinates */
void set_mouse_coord(signed long x,signed long y)
{
  if(x<0) x = 0; else if(x>255) x = 255;
  if(y<0) y = 0; else if(y>192) y = 192;
  mouse.x = x;
  mouse.y = y;
}

/* Update NDS keypad */
void update_keypad(u16 keys)
{
  ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] = ~keys & 0x3FF;
  ((u16 *)MMU.ARM7_REG)[0x130>>1] = ~keys & 0x3FF;
  /* Update X and Y buttons */
  MMU.ARM7_REG[0x136] = ( ~( keys >> 10) & 0x3 ) | (MMU.ARM7_REG[0x136] & ~0x3);
}

/* Retrieve current NDS keypad */
u16 get_keypad( void)
{
  u16 keypad;
  keypad = ~MMU.ARM7_REG[0x136];
  keypad = (keypad & 0x3) << 10;
  keypad |= ~((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] & 0x3FF;
  return keypad;
}

/*
 * The internal joystick events processing function
 */
static int
do_process_joystick_events( u16 *keypad, SDL_Event *event) {
  int processed = 1;
  u16 key;

  switch ( event->type)
    {
      /* Joystick axis motion 
         Note: button constants have a 1bit offset. */
    case SDL_JOYAXISMOTION:
      key = lookup_joy_key( JOY_AXIS_(event->jaxis.axis) );
      if( key == 0 ) break;           /* Not an axis of interest? */

      /* Axis is back to initial position */
      if( event->jaxis.value == 0 )
        RM_KEY( *keypad, key | get_joy_axis_twin(key) );
      /* Key should have been down but its currently set to up? */
      else if( (event->jaxis.value > 0) && 
               (key == KEYMASK_( KEY_UP-1 )) )
        key = KEYMASK_( KEY_DOWN-1 );
      /* Key should have been left but its currently set to right? */
      else if( (event->jaxis.value < 0) && 
               (key == KEYMASK_( KEY_RIGHT-1 )) )
        key = KEYMASK_( KEY_LEFT-1 );
              
      /* Remove some sensitivity before checking if different than zero... 
         Fixes some badly behaving joypads [like one of mine]. */
      if( (event->jaxis.value >> 5) != 0 )
        ADD_KEY( *keypad, key );
      break;

      /* Joystick button pressed */
      /* FIXME: Add support for BOOST */
    case SDL_JOYBUTTONDOWN:
      key = lookup_joy_key( event->jbutton.button );
      ADD_KEY( *keypad, key );
      break;

      /* Joystick button released */
    case SDL_JOYBUTTONUP:
      key = lookup_joy_key(event->jbutton.button);
      RM_KEY( *keypad, key );
      break;

    default:
      processed = 0;
      break;
    }

  return processed;
}

/*
 * Process only the joystick events
 */
void
process_joystick_events( u16 *keypad) {
  SDL_Event event;

  /* IMPORTANT: Reenable joystick events iif needed. */
  if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
    SDL_JoystickEventState(SDL_ENABLE);

  /* There's an event waiting to be processed? */
  while (SDL_PollEvent(&event))
    {
      do_process_joystick_events( keypad, &event);
    }
}

/* Manage input events */
int
process_ctrls_events( u16 *keypad,
                      void (*external_videoResizeFn)( u16 width, u16 height),
                      float nds_screen_size_ratio)
{
  
//HCF
  int i, iMouseCambiado;
  signed short x, y;
  unsigned short key;

  int cause_quit = 0;
  
  //HCF Botones
  vdRellenaBotones();
  for(i=0;i<12;i++) 
  {
	//HCF De momento solo horizontal
	/****	
	if(vertical)
	{
		if (ashBotones[default_xb_cfg_v[i]])
	  //if (pad.Buttons & default_psp_cfg_v[i])
			ADD_KEY( *keypad, KEYMASK_(i));
		else
			RM_KEY( *keypad, KEYMASK_(i));
	}
	else
	{
	****/
		//if (pad.Buttons & default_psp_cfg_h[i])
		if(ashBotones[default_xb_cfg_h[i]])
			ADD_KEY( *keypad, KEYMASK_(i));
		else
			RM_KEY( *keypad, KEYMASK_(i));
	/***}****/
  }   

  //HCF Movimiento del puntero
		x = SDL_JoystickGetAxis(GAMEPAD, 0),
		y = SDL_JoystickGetAxis(GAMEPAD, 1);
	
		iMouseCambiado = 0;
		if (x > 16384)
		{
			
			if( mouse.x < 256 - iMouseSpeed )
			{
				mouse.x += iMouseSpeed;
				iMouseCambiado = 1;
			}
		}
		if (x < -16384)
		{
			
			if( mouse.x >= iMouseSpeed )
			{
				mouse.x -= iMouseSpeed;
				iMouseCambiado = 1;
			}
		}
		if (y > 16384)
		{
			if( mouse.y < 192 - iMouseSpeed )
			{
				mouse.y += iMouseSpeed;
				iMouseCambiado = 1;
			}
		}
		if (y < -16384)
		{
			if( mouse.y >= iMouseSpeed )
			{
				mouse.y -= iMouseSpeed;
				iMouseCambiado = 1;
			}
		}

		if(iMouseCambiado)
		{
			switch(iModoGrafico)
			{
				case 2:
				case 1:
					//HCF Solo pantalla 1 o 2
					SDL_WarpMouse(mouse.x + PANTALLA2_UNICA_X, mouse.y + PANTALLA2_UNICA_Y);
					break;
				case 0:
				default:
					//HCF Una pantalla encima de otra
					switch(iModoStretchNuevo)
					{
						case STRETCH_MODE_FULL:
							SDL_WarpMouse(mouse.x * 2, mouse.y + PANTALLA2_Y);
							break;
						case STRETCH_MODE_HALF:
							SDL_WarpMouse((mouse.x * 1.5) + (PANTALLA2_X / 2), mouse.y + PANTALLA2_Y);
							break;
						case STRETCH_MODE_NONE:
						default:
							SDL_WarpMouse(mouse.x + PANTALLA2_X, mouse.y + PANTALLA2_Y);
							break;
					}
					break;
			}
		}
	//HCF Movimiento del puntero

	//HCF Click o Drag and drop
	if (ashBotones[BOTON_BLANCO]) 
	{
		mouse.down = TRUE;
	}
	else
	{
		if(mouse.down == TRUE)
		{
			mouse.click = TRUE;
			mouse.down = FALSE;	
		}
		else
		{
			mouse.click = FALSE;
		}
	}
	//HCF Click o Drag and drop

	//HCF Toggle graphical mode
	if (ashBotones[BOTON_NEGRO]) 
	{
		if( !iCambiandoModoGrafico )
		{
			iModoGraficoNuevo = ((iModoGrafico+1)%3);
			iCambiandoModoGrafico = 1;

			switch(iModoGraficoNuevo)
			{
				case 2:
				case 1:
					//HCF Solo pantalla 1 o 2
					SDL_WarpMouse(mouse.x + PANTALLA2_UNICA_X, mouse.y + PANTALLA2_UNICA_Y);
					break;
				case 0:
				default:
					//HCF Una pantalla encima de otra
					switch(iModoStretchNuevo)
					{
						case STRETCH_MODE_FULL:
							SDL_WarpMouse(mouse.x * 2, mouse.y + PANTALLA2_Y);
							break;
						case STRETCH_MODE_HALF:
							SDL_WarpMouse((mouse.x * 1.5) + (PANTALLA2_X / 2), mouse.y + PANTALLA2_Y);
							break;
						case STRETCH_MODE_NONE:
						default:
							SDL_WarpMouse(mouse.x + PANTALLA2_X, mouse.y + PANTALLA2_Y);
							break;
					}
					break;
			}
		}
	}
	else if(iCambiandoModoGrafico)
	{
		iCambiandoModoGrafico = 0;
	}

	//HCF Toggle stretch mode
	if (ashBotones[BOTON_RTHUMBSTICK]) 
	{
		if( !iCambiandoModoStretch )
		{
			iModoStretchNuevo = ((iModoStretch+1)%3);
			iCambiandoModoStretch = 1;

			//HCF This only matters in 2 screen mode
			if( iModoGraficoNuevo == 0 )
			{
				//HCF Una pantalla encima de otra
				switch(iModoStretchNuevo)
				{
					case STRETCH_MODE_FULL:
						SDL_WarpMouse(mouse.x * 2, mouse.y + PANTALLA2_Y);
						break;
					case STRETCH_MODE_HALF:
						SDL_WarpMouse((mouse.x * 1.5) + (PANTALLA2_X / 2), mouse.y + PANTALLA2_Y);
						break;
					case STRETCH_MODE_NONE:
					default:
						SDL_WarpMouse(mouse.x + PANTALLA2_X, mouse.y + PANTALLA2_Y);
						break;
				}
			}
		}
	}
	else if(iCambiandoModoStretch)
	{
		iCambiandoModoStretch = 0;
	}

  //HCF Puntero
  /*
  if (ashBotones[BOTON_BLANCO]) 
  //else
  {
	  if (ashBotones[BOTON_NEGRO]) 
	  {
		  //Si esta pulsando y clickando (drag and drop)
	  	  mouse.down = TRUE;
	  }
	  
	  //HCF De momento usamos arriba y abajo en PAD... ver si usar JOY/PAD o solo JOY...
	  if (ashBotones[BOTON_IZQUIERDA])
	  {
		//--mouse.x;
		mouse.x -= MOUSE_SPEED;
	  } 
	  if (ashBotones[BOTON_DERECHA])
	  {
		//++mouse.x;
		mouse.x += MOUSE_SPEED;
	  } 
      if (ashBotones[BOTON_ARRIBA])
	  {
		//--mouse.y;
		mouse.y -= MOUSE_SPEED;
	  } 
	  if (ashBotones[BOTON_ABAJO])
	  {
		//++mouse.y;
		mouse.y += MOUSE_SPEED;
	  }
		 
	  SDL_WarpMouse(mouse.x, mouse.y);
      set_mouse_coord( mouse.x, mouse.y );
  }
  else
  {
	if (ashBotones[BOTON_NEGRO] ) 
  	{
		//Click sin arrastrar
		 mouse.click = TRUE;
		 //mouse.down = FALSE;
	  }
  }
  */

  //HCF In-game menu
  if ( (!ashBotones[BOTON_START]) && (ashBotones[BOTON_BACK]) )
      vdXBOptionsMenu(0);

  //HCF Salir
  if (ashBotones[BOTON_START] && ashBotones[BOTON_BACK]) 
  	cause_quit = 1;
  
  /*************
  #ifdef __psp__
	  SceCtrlData pad;
	  sceCtrlSetSamplingCycle(0);
	  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	  sceCtrlPeekBufferPositive(&pad, 1); 
	  
	  int i;
	  for(i=0;i<12;i++) {
	if(vertical){
	  if (pad.Buttons & default_psp_cfg_v[i])
			ADD_KEY( *keypad, KEYMASK_(i));
		else
			RM_KEY( *keypad, KEYMASK_(i));
	  }else{
		if (pad.Buttons & default_psp_cfg_h[i])
			ADD_KEY( *keypad, KEYMASK_(i));
		else
			RM_KEY( *keypad, KEYMASK_(i));
	  }
	  }    
	  if (pad.Buttons & PSP_CTRL_NOTE) {
	  
	  		mouse.click = TRUE;
			mouse.down = FALSE;
	  }
	  else
	  {
		  mouse.down = TRUE;
		  if(vertical){
          if (pad.Ly < 10){
			--mouse.x;
		  } 
			
		  if (pad.Ly > 245){
			++mouse.x;
		  } 
		  
		  if (pad.Lx < 10) {
			++mouse.y;
		  } 
		
		  if (pad.Lx > 245){
			--mouse.y;
		  }
		  }else{

		  if (pad.Lx < 10){
			--mouse.x;
		  } 
			
		  if (pad.Lx > 245){
			++mouse.x;
		  } 
		  
		  if (pad.Ly < 10) {
			--mouse.y;
		  } 
		
		  if (pad.Ly > 245){
			++mouse.y;
		  }
		  }
		  /******** HCF ESTABA YA COMENTADO **********
		        signed long scaled_x =
					screen_to_touch_range_x( event.button.x,
											 nds_screen_size_ratio);
				  signed long scaled_y =
					screen_to_touch_range_y( event.button.y,
											 nds_screen_size_ratio);
	
				  if( scaled_y >= 192)
					set_mouse_coord( scaled_x, scaled_y - 192);
				}
			******* HCF ESTABA YA COMENTADO ************
        //  SDL_WarpMouse(mouse.x, mouse.y);
		  set_mouse_coord( mouse.x, mouse.y );
	  }
	    
  #else		
	  SDL_Event event;
	  
	  // IMPORTANT: Reenable joystick events if needed
	  if(SDL_JoystickEventState(SDL_QUERY) == SDL_IGNORE)
		SDL_JoystickEventState(SDL_ENABLE);
	
	  // There's an event waiting to be processed?
	  while (SDL_PollEvent(&event))
		{
		  if ( !do_process_joystick_events( keypad, &event)) {
			switch (event.type)
			  {
			  case SDL_VIDEORESIZE:
				if ( external_videoResizeFn != NULL) {
				  external_videoResizeFn( event.resize.w, event.resize.h);
				}
				break;
	
			  case SDL_KEYDOWN:
				key = lookup_key(event.key.keysym.sym);
				ADD_KEY( *keypad, key );
				break;
	
			  case SDL_KEYUP:
				key = lookup_key(event.key.keysym.sym);
				RM_KEY( *keypad, key );
				break;
	
			  case SDL_MOUSEBUTTONDOWN:
				if(event.button.button==1)
				  mouse.down = TRUE;
							
			  case SDL_MOUSEMOTION:
				if(!mouse.down)
				  break;
				else {
				  signed long scaled_x =
					screen_to_touch_range_x( event.button.x,
											 nds_screen_size_ratio);
				  signed long scaled_y =
					screen_to_touch_range_y( event.button.y,
											 nds_screen_size_ratio);
	
				  if( scaled_y >= 192)
					set_mouse_coord( scaled_x, scaled_y - 192);
				}
				break;
	
			  case SDL_MOUSEBUTTONUP:
				if(mouse.down) mouse.click = TRUE;
				mouse.down = FALSE;
				break;
	
			  case SDL_QUIT:
				cause_quit = 1;
				break;
	
			  default:
				break;
			  }
			}
		}
	#endif
	***********/

  return cause_quit;
}

