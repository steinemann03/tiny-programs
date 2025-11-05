#include <windows.h>
#include <math.h>

#define MAX_BALLS 999 //hehe

struct programState{
	int running;
	int paused;
	int mX, mY;
	int mPressed, mReleased, mHeld;
	float windowDimensions[2];
	//
};
struct programState programState = { 0 };
void programTogglePaused(){
	programState.paused = !programState.paused;
}

struct programButton{
	int offsetPos[2];
	int size[2];
	void* func;
	//
	char* txt;
	int txt_len;
};
void buttonInit(struct programButton* button, int pos[2], int size[2], void* func_ptr, char* txt, int txt_len){
	button->offsetPos[0] = pos[0];
	button->offsetPos[1] = pos[1];
	button->size[0] = size[0];
	button->size[1] = size[1];
	button->func = func_ptr;
	button->txt = txt;
	button->txt_len = txt_len; //could just calculate this and not have to pass it, i dont remember the func on top of my head lol
}
void buttonExec(struct programButton* button){
	if (button->func != NULL){
		void (*func_ptr)(void) = button->func;
		func_ptr();	
	}
}
int buttonRegister(struct programButton* button, int menuPos[2], int* success){
	if (programState.mX < button->offsetPos[0] + menuPos[0] + button->size[0] &&
		programState.mX > button->offsetPos[0] + menuPos[0] &&
		programState.mY < button->offsetPos[1] + menuPos[1] + button->size[1] &&
		programState.mY > button->offsetPos[1] + menuPos[1] ){
		buttonExec(button);
		*success = 1;
	}
}

#define MENU_SIZE_X 100
#define MENU_SIZE_Y 200
#define MENU_BUTTONS 3
struct programMenu{
	int pos[2];
	struct programButton buttons[MENU_BUTTONS];
	//
	int move;
	int mouseOffset[2];
};
struct programMenu programMenu = { 0 };

void menuInit(){
	programMenu.pos[0] = 100;
	programMenu.pos[1] = 100;
	//buttons need to be initialized in main
}
void menuLoopState(){
	if (!programState.mHeld)
		programMenu.move = 0;
	if (programMenu.move){
		programMenu.pos[0] = programState.mX + programMenu.mouseOffset[0];
		programMenu.pos[1] = programState.mY + programMenu.mouseOffset[1];
	}
}
void menuRegisterInput(){
	if (programState.mX >= programMenu.pos[0] &&
		programState.mX <= programMenu.pos[0] + MENU_SIZE_X &&
		programState.mY >= programMenu.pos[1] &&
		programState.mY <= programMenu.pos[1] + MENU_SIZE_Y){
		int success = 0;
		for (int i = 0; i < MENU_BUTTONS; i++){
			buttonRegister(&programMenu.buttons[i], programMenu.pos, &success);
		}
		if (!success){
			programMenu.mouseOffset[0] = programMenu.pos[0] - programState.mX;
			programMenu.mouseOffset[1] = programMenu.pos[1] - programState.mY;
			programMenu.move = 1;
		}
	}
}

//
typedef struct{
	float f[2];
} vec2;
float deltaTime = 0.1;

//objects
typedef struct{
	int active;
	vec2 pos;
	float radius;
	vec2 vel;
	vec2 acc;
} rigidBody;
rigidBody rigidBodies[MAX_BALLS] = { 0 };

void rigidBody_clear(){
	for (int i = 0; i < MAX_BALLS; i++)
		rigidBodies[i].active = 0;
}
void rigidBody_add(){
	for (int j = 0; j < MAX_BALLS; j++){
		if (rigidBodies[j].active == 0){
			rigidBodies[j] = (rigidBody){
				1,
				(vec2){programState.windowDimensions[0] / 2, programState.windowDimensions[1] / 2},
				20,
				(vec2){0,0},
				(vec2){0,0}
			};
			break;
		}
	}	
}

void rigidBody_update(rigidBody *r){
	float constant_friction = 0.99;
	r->pos.f[0] += r->vel.f[0];
	r->pos.f[1] += r->vel.f[1];
	r->vel.f[0] *= constant_friction;
	r->vel.f[1] *= constant_friction;
}
void rigidBody_applyGravity(rigidBody *r){
	r->vel.f[1] += 9.8 * deltaTime;
}
void rigidBody_wallBounce(rigidBody *r){
	float restitution = 0.9; // sets bounce of wall
	if (r->pos.f[0] - r->radius < 0){ //left wall
		r->pos.f[0] = r->radius;
		r->vel.f[0] *= -restitution;
	}
	if (r->pos.f[0] + r->radius > programState.windowDimensions[0]){ //right wall
		r->pos.f[0] = programState.windowDimensions[0] - r->radius;
		r->vel.f[0] *= -restitution;
	}
	if (r->pos.f[1] - r->radius < 0){ //top wall
		r->pos.f[1] = r->radius;
		r->vel.f[1] *= -restitution;
	}
	if (r->pos.f[1] + r->radius > programState.windowDimensions[1]){ //bottom wall
		r->pos.f[1] = programState.windowDimensions[1] - r->radius;
		r->vel.f[1] *= -restitution;
	}
}
void rigidBody_collision(rigidBody *r1, rigidBody *r2){
	float delta_x = r2->pos.f[0] - r1->pos.f[0];
	float delta_y = r2->pos.f[1] - r1->pos.f[1];
	float distance = delta_x * delta_x + delta_y * delta_y;
	float radius_sum = r1->radius + r2->radius;
	if (distance < radius_sum * radius_sum){
		distance = sqrt(distance);
		if (distance == 0) return;
		float normalized_x = delta_x / distance;
		float normalized_y = delta_y / distance;
		
		float relative_velocity_x = r2->vel.f[0] - r1->vel.f[0];
		float relative_velocity_y = r2->vel.f[1] - r1->vel.f[1];
		
		float dot_product = relative_velocity_x * normalized_x + relative_velocity_y * normalized_y;
		if (dot_product > 0) return;
		
		float restitution = 0.9;
		float mass_r1 = r1->radius;
		float mass_r2 = r2->radius;
		float mass = 2;
		float impulse = (-(1 + restitution) * dot_product) / mass;
		
		r1->vel.f[0] -= impulse * normalized_x;
		r1->vel.f[1] -= impulse * normalized_y;
		r2->vel.f[0] += impulse * normalized_x;
		r2->vel.f[1] += impulse * normalized_y;
	}
}

LRESULT CALLBACK proc(HWND window, UINT msg, WPARAM wp, LPARAM lp);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR lpCmdLine, int nCmdShow){
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = proc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "physics";
	
	RegisterClass(&wc);
	
	HWND window = CreateWindow(
		"physics",
		"physics simulator",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 600, 600,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	if (window == NULL)
		return 0;
	programState.windowDimensions[0] = 600;
	programState.windowDimensions[1] = 600;
	
	ShowWindow(window, nCmdShow);
	UpdateWindow(window);
	
	//init
	programState.running = 1;
	programState.paused = 0;
	
	menuInit();
	{
		int bpos[2] = { 10, 10 };
		int bsize[2] = { 75, 25 };
		buttonInit(&programMenu.buttons[0], bpos, bsize, rigidBody_add, "Add Ball", 8);
		bpos[1] += 50;
		buttonInit(&programMenu.buttons[1], bpos, bsize, rigidBody_clear, "Clear Balls", 11);
		bpos[1] += 50;
		buttonInit(&programMenu.buttons[2], bpos, bsize, programTogglePaused, "Pause", 5);
	}
	
	rigidBodies[0] = (rigidBody){
		1,
		(vec2){500,100},
		20,
		(vec2){40,0},
		(vec2){0,0}
	};
	rigidBodies[1] = (rigidBody){
		1,
		(vec2){100,100},
		20,
		(vec2){-40,0},
		(vec2){0,0}
	};
	int selected_rigidBody = -1;
	
	MSG msg;
	while (programState.running){
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
			if (msg.message == WM_QUIT)
				programState.running = 0;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		menuLoopState();
		
		//move or spawn rigid bodies
		if (programState.mReleased)
			selected_rigidBody = -1;
		if (programState.mPressed)
			for (int i = 0; i < MAX_BALLS; i++){
				if (programState.mX > rigidBodies[i].pos.f[0] - rigidBodies[i].radius &&
					programState.mX < rigidBodies[i].pos.f[0] + rigidBodies[i].radius &&
					programState.mY > rigidBodies[i].pos.f[1] - rigidBodies[i].radius &&
					programState.mY < rigidBodies[i].pos.f[1] + rigidBodies[i].radius)
					selected_rigidBody = i;
			}
		if (programState.mHeld)
			if (selected_rigidBody == -1){
				if (programState.mPressed){
					/*
					for (int j = 0; j < MAX_BALLS; j++){
						if (rigidBodies[j].active == 0){
							rigidBodies[j] = (rigidBody){
								1,
								(vec2){programState.mX, programState.mY},
								20,
								(vec2){0,0},
								(vec2){0,0}
							};
							break;
						}
					}	
					*/		
					menuRegisterInput();
				}
			} else {
				rigidBodies[selected_rigidBody].pos.f[0] = programState.mX;
				rigidBodies[selected_rigidBody].pos.f[1] = programState.mY;
			}
		
		//logic loop
		if (programState.paused != 1){
			//apply forces
			for (int i = 0; i < MAX_BALLS; i++){
				if (rigidBodies[i].active && i != selected_rigidBody)
					rigidBody_applyGravity(&rigidBodies[i]);
			}
			//update bodies
			for (int i = 0; i < MAX_BALLS; i++){
				if (rigidBodies[i].active)
					rigidBody_update(&rigidBodies[i]);
			}
			//check and apply collisions
			for (int i = 0; i < MAX_BALLS; i++){
				if (rigidBodies[i].active){
					rigidBody_wallBounce(&rigidBodies[i]);
					for (int j = 0; j < MAX_BALLS; j++)
						if (rigidBodies[i].pos.f[0] + rigidBodies[i].radius * 2 > rigidBodies[i].pos.f[0] - rigidBodies[i].radius * 2 &&
							rigidBodies[i].pos.f[0] - rigidBodies[i].radius * 2 < rigidBodies[i].pos.f[0] + rigidBodies[i].radius * 2 &&
							rigidBodies[i].pos.f[1] + rigidBodies[i].radius * 2 > rigidBodies[i].pos.f[1] - rigidBodies[i].radius * 2 &&
							rigidBodies[i].pos.f[1] - rigidBodies[i].radius * 2 < rigidBodies[i].pos.f[1] + rigidBodies[i].radius * 2)
							rigidBody_collision(&rigidBodies[i], &rigidBodies[j]);
				}
			}
			//rigidBody_collision(&rigidBodies[0], &rigidBodies[1]);
		}
		
		programState.mPressed = 0;
		programState.mReleased = 0;
		//
		InvalidateRect(window, NULL, FALSE);
		Sleep(16);	
	}
	return msg.wParam;
}

LRESULT CALLBACK proc(HWND window, UINT message, WPARAM wp, LPARAM lp){
	static HBITMAP hbmBackBuffer = NULL;
    static HDC hdcBackBuffer = NULL;
	switch(message){
		case WM_CREATE: {
			//we can just run this in our draw loop because then it supports window resizing
			//or use this call and the WM_RESIZE or whatever, idc
			
			//HDC hdc = GetDC(window);
            //hdcBackBuffer = CreateCompatibleDC(hdc);
            //hbmBackBuffer = CreateCompatibleBitmap(hdc, programState.windowDimensions[0], programState.windowDimensions[1]);
            //SelectObject(hdcBackBuffer, hbmBackBuffer);
            //ReleaseDC(window, hdc);
			break;
		}
		case WM_DESTROY:
			DeleteDC(hdcBackBuffer);
            DeleteObject(hbmBackBuffer);
			PostQuitMessage(0);
			break;
		case WM_KEYDOWN:
			if (VK_ESCAPE == wp)
				DestroyWindow(window);
			if (VK_SPACE == wp)
				if (programState.paused == 0)
					programState.paused = 1;
				else
					programState.paused = 0;
			if (VK_UP == wp)
				deltaTime += 0.01;
			if (VK_DOWN == wp)
				deltaTime -= 0.01;
			break;
		case WM_MOUSEMOVE:
			programState.mX = LOWORD(lp);
			programState.mY = HIWORD(lp);
			break;
		case WM_LBUTTONDOWN:
			programState.mPressed = 1;
			programState.mHeld = 1;
			break;
		case WM_LBUTTONUP:
			programState.mReleased = 1;
			programState.mHeld = 0;
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(window, &ps);
			
			hdcBackBuffer = CreateCompatibleDC(hdc);
            hbmBackBuffer = CreateCompatibleBitmap(hdc, programState.windowDimensions[0], programState.windowDimensions[1]);
            SelectObject(hdcBackBuffer, hbmBackBuffer);

			// Background
			RECT rc;
			GetClientRect(window, &rc);
			HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0)); // background color
			FillRect(hdcBackBuffer, &rc, hBrush);
			DeleteObject(hBrush);  // Free background brush
			
			// set program window dimensions
			int width = rc.right;
			int height = rc.bottom;
			programState.windowDimensions[0] = width;
			programState.windowDimensions[1] = height;

			//draw rigid bodies
			HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255,255,255));
			SelectObject(hdcBackBuffer, hPen);
			hBrush = SelectObject(hdcBackBuffer, GetStockObject(NULL_BRUSH));
			for (int i = 0; i < MAX_BALLS; i++){
				if (rigidBodies[i].active == 0)
					continue;
				Ellipse(hdcBackBuffer,
					rigidBodies[i].pos.f[0] - rigidBodies[i].radius,
					rigidBodies[i].pos.f[1] - rigidBodies[i].radius,
					rigidBodies[i].pos.f[0] + rigidBodies[i].radius,
					rigidBodies[i].pos.f[1] + rigidBodies[i].radius
				);
			}
			
			DeleteObject(hPen);
			DeleteObject(hBrush);
			
			hBrush = CreateSolidBrush(RGB(0,0,0));
			hPen = CreatePen(PS_SOLID, 1, RGB(255,255,255));
			SelectObject(hdcBackBuffer, hBrush);
			SelectObject(hdcBackBuffer, hPen);
			
			/*
			Rectangle(hdcBackBuffer,
				programMenu.pos[0],
				programMenu.pos[0] + MENU_SIZE_X,
				programMenu.pos[1],
				programMenu.pos[1] + MENU_SIZE_Y
			);
			*/
			Rectangle(
				hdcBackBuffer,
				programMenu.pos[0], 
				programMenu.pos[1], 
				programMenu.pos[0] + MENU_SIZE_X, 
				programMenu.pos[1] + MENU_SIZE_Y
				);
			for (int i = 0; i < MENU_BUTTONS; i++){
				RECT brc = { 
					programMenu.pos[0] + programMenu.buttons[i].offsetPos[0],
					programMenu.pos[1] + programMenu.buttons[i].offsetPos[1],
					programMenu.pos[0] + programMenu.buttons[i].offsetPos[0] + programMenu.buttons[i].size[0],
					programMenu.pos[1] + programMenu.buttons[i].offsetPos[1] + programMenu.buttons[i].size[1]
					};
				Rectangle(
					hdcBackBuffer,
					brc.left,
					brc.top,
					brc.right,
					brc.bottom
					);
					DrawText(hdcBackBuffer, programMenu.buttons[i].txt, -1, &brc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
			
			DeleteObject(hPen);
			DeleteObject(hBrush);
			
			//display text
			if (programState.paused)
				DrawText(hdcBackBuffer, "paused", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

			BitBlt(hdc, 0, 0, programState.windowDimensions[0], programState.windowDimensions[1], hdcBackBuffer, 0, 0, SRCCOPY);
			
			DeleteObject(hdcBackBuffer);
			DeleteObject(hbmBackBuffer);
			
			EndPaint(window, &ps);
			break;
		}
		default:
            return DefWindowProc(window, message, wp, lp);
	}
	return 0;
}

