#include <Windows.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

typedef struct {
	float f[2];
}vec2;
typedef struct {
	float f[3];
}vec3;
typedef struct {
	float f[4];
}vec4;
vec2 rotate2d(float i, float j, float rad) {
	return (vec2) {
		i* cos(rad) - j * sin(rad),
		j* cos(rad) + i * sin(rad)
	};
}
vec3 vec3_subtract(vec3 v1, vec3 v2) {
	return (vec3) {
		v1.f[0] - v2.f[0],
			v1.f[1] - v2.f[1],
			v1.f[2] - v2.f[2]
	};
}
vec3 vec3_cross(vec3 a, vec3 b) {
	vec3 c;
	c.f[0] = a.f[1] * b.f[2] - a.f[2] * b.f[1];
	c.f[1] = a.f[2] * b.f[0] - a.f[0] * b.f[2];
	c.f[2] = a.f[0] * b.f[1] - a.f[1] * b.f[0];
	return c;
}

//program
#define DEF_RES 800
#define drawFreq 32768
#define inputFreq 8192
#define dscaling 1
struct program {
	int running;
	int res[2];
	//
	int draw_tick;
	int input_tick;
	//
	int debug_draw;
	int m_active;
	int culling;
};
struct program* program = NULL;
void initProgram() {
	program = malloc(sizeof(struct program));
	if (!program)
		exit(1);
	program->running = 1;
	program->res[0] = DEF_RES;
	program->res[1] = DEF_RES;
	//
	program->draw_tick = 0;
	program->input_tick = 0;
	//
	program->debug_draw = 0;
	program->m_active = 0;
	program->culling = 0; //buggy
}

#define c_defaultzoom 40
struct camera {
	vec3 pos;
	vec3 rot;
	float zoom;
	int freemode;
};
struct camera* camera = NULL;
void initCamera() {
	camera = malloc(sizeof(struct camera));
	if (!camera)
		exit(1);
	camera->rot = (vec3){ 0 };
	camera->pos = (vec3){ 0,0,-5 };
	camera->zoom = c_defaultzoom;
	camera->freemode = 1;
}

//
HBRUSH global_colors[6];
HBRUSH shading_colors[4];

#define max_world_meshes 99999
#define max_visual_meshes 4086
#define c_thresshold 0
#define draw_dist 300
typedef struct {
	int index;
	vec3 verticies[3];
	HBRUSH color;
}world_mesh;
typedef struct {
	int index;
	POINT points[3];
	float depth;
	float color_fade;
	HBRUSH final_color;
}visual_mesh;
world_mesh* world_meshes[max_world_meshes] = { 0 };
visual_mesh* visual_meshes[max_visual_meshes] = { 0 };

void initMeshes() {
	for (int i = 0; i < max_world_meshes; i++) {
		world_meshes[i] = calloc(1, sizeof(world_mesh));
		world_meshes[i]->color = NULL;
	}
	for (int i = 0; i < max_visual_meshes; i++) {
		visual_meshes[i] = calloc(1, sizeof(visual_mesh));
		visual_meshes[i]->final_color = NULL;
	}
}

int addWorldMesh(world_mesh mesh) {
	int available_index = -1;
	for (int i = 0; i < max_world_meshes; i++)
		if (world_meshes[i]->color == NULL) {
			available_index = i;
			break;
		}
	if (available_index == -1)
		return 0;
	*world_meshes[available_index] = mesh;
	world_meshes[available_index]->index = available_index;
	return 1;
}
int removeWorldMesh(int index) {
	if (index > 0 || index < max_world_meshes)
		return 0;
	if (world_meshes[index]->color == NULL)
		return 0;
	else
		world_meshes[index]->color = NULL;
	return 1;
}
void removeAllWorldMeshes() {
	for (int i = 0; i < max_world_meshes; i++)
		world_meshes[i]->color = NULL;
}

int addVisualMesh(visual_mesh mesh) {
	int available_index = -1;
	for (int i = 0; i < max_visual_meshes; i++)
		if (visual_meshes[i]->final_color == NULL) {
			available_index = i;
			break;
		}
	if (available_index == -1)
		return 0;
	*visual_meshes[available_index] = mesh;
	visual_meshes[available_index]->index = available_index;
	return 1;
}
int removeVisualMesh(int index) {
	if (index > 0 || index < max_visual_meshes)
		return 0;
	if (visual_meshes[index]->final_color == NULL)
		return 0;
	else
		visual_meshes[index]->final_color = NULL;
	return 1;
}
void removeAllVisualMeshes() {
	for (int i = 0; i < max_visual_meshes; i++) {
		visual_meshes[i]->final_color = NULL;
	}
}

#define terrain_size 64
#define grid_size 8
float terrain[terrain_size][terrain_size] = { 0 };
void initTerrain() {
	for (int x = 0; x < terrain_size; x++) {
		for (int z = 0; z < terrain_size; z++) {
			terrain[x][z] = (float)(rand() % 200 - 100) / 100.0f;
		}
	}

	// Smooth passes
	for (int i = 0; i < 3; i++) { // Do 3 smoothing passes
		for (int x = 1; x < terrain_size - 1; x++) {
			for (int z = 1; z < terrain_size - 1; z++) {
				terrain[x][z] = (
					terrain[x][z] +
					terrain[x - 1][z] +
					terrain[x + 1][z] +
					terrain[x][z - 1] +
					terrain[x][z + 1]
					) / 3.0f;
			}
		}
	}
}
int color_selection[2] = { 0 };
void terrainBuild() {
	removeAllWorldMeshes();
	for (int x = 0; x < terrain_size; x++) {
		for (int z = 0; z < terrain_size; z++) {
			if (x < terrain_size - 1 && z < terrain_size - 1) {
				world_mesh temp1 = {
					.verticies = {
						{x * grid_size, terrain[x][z] * grid_size, z * grid_size},                    // A
						{(x + 1) * grid_size, terrain[x + 1][z + 1] * grid_size, (z + 1) * grid_size}, // D
						{(x + 1) * grid_size, terrain[x + 1][z] * grid_size, z * grid_size}           // C
					},
					.color = global_colors[color_selection[0]]
				};
				addWorldMesh(temp1);

				world_mesh temp2 = {
					.verticies = {
						{x * grid_size, terrain[x][z] * grid_size, z * grid_size},                    // A
						{x * grid_size, terrain[x][z + 1] * grid_size, (z + 1) * grid_size},          // B
						{(x + 1) * grid_size, terrain[x + 1][z + 1] * grid_size, (z + 1) * grid_size} // D
					},
					.color = global_colors[color_selection[1]]
				};
				addWorldMesh(temp2);
			}
		}
	}
}

void terrainWaves() {
	for (int x = 0; x < terrain_size; x++)
		for (int z = 0; z < terrain_size; z++) {
			terrain[x][z] += 0.01f * (float) { rand() % 10 - 5 };
		}
	terrainBuild();
}

//lol
int pointInTriangleXZ(vec3 p, vec3 a, vec3 b, vec3 c) {
	float area = 0.5f * (-b.f[2] * c.f[0] + a.f[2] * (-b.f[0] + c.f[0]) + a.f[0] * (b.f[2] - c.f[2]) + b.f[0] * c.f[2]);
	float s = 1.0f / (2.0f * area) * (a.f[2] * c.f[0] - a.f[0] * c.f[2] + (c.f[2] - a.f[2]) * p.f[0] + (a.f[0] - c.f[0]) * p.f[2]);
	float t = 1.0f / (2.0f * area) * (a.f[0] * b.f[2] - a.f[2] * b.f[0] + (a.f[2] - b.f[2]) * p.f[0] + (b.f[0] - a.f[0]) * p.f[2]);
	return (s >= 0) && (t >= 0) && (1 - s - t >= 0);
}
float barycentricY(vec3 p, vec3 a, vec3 b, vec3 c) {
	float det = (b.f[2] - c.f[2]) * (a.f[0] - c.f[0]) + (c.f[0] - b.f[0]) * (a.f[2] - c.f[2]);
	float l1 = ((b.f[2] - c.f[2]) * (p.f[0] - c.f[0]) + (c.f[0] - b.f[0]) * (p.f[2] - c.f[2])) / det;
	float l2 = ((c.f[2] - a.f[2]) * (p.f[0] - c.f[0]) + (a.f[0] - c.f[0]) * (p.f[2] - c.f[2])) / det;
	float l3 = 1.0f - l1 - l2;
	return l1 * a.f[1] + l2 * b.f[1] + l3 * c.f[1];
}


LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);

WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR cmdLine, int showCmd) {
	srand(time(NULL));
	initProgram();
	initMeshes();
	initCamera();
	global_colors[0] = CreateSolidBrush(RGB(255, 0, 0));
	global_colors[1] = CreateSolidBrush(RGB(0, 255, 0));
	global_colors[2] = CreateSolidBrush(RGB(0, 0, 255));
	global_colors[3] = CreateSolidBrush(RGB(255, 255, 0));
	global_colors[4] = CreateSolidBrush(RGB(0, 255, 255));
	global_colors[5] = CreateSolidBrush(RGB(255, 0, 255));

	//tests
	initTerrain();
	terrainBuild();

	WNDCLASS window_class = { 0 };
	window_class.lpfnWndProc = proc;
	window_class.hInstance = hInstance;
	window_class.lpszClassName = "lol";
	RegisterClass(&window_class);

	HWND window = CreateWindow("lol", "renderer",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		DEF_RES, DEF_RES, NULL, NULL, hInstance, NULL
	);
	ShowWindow(window, showCmd);
	UpdateWindow(window);

	{
		world_mesh temp1 = {
			.verticies = { {1,1,-1}, {1,-1,-1}, {-1,-1,-1} },
			.color = global_colors[0]
		};
		addWorldMesh(temp1);
	}
	{
		world_mesh temp1 = {
			.verticies = { {1,1,-1}, {-1,1,-1}, {-1,-1,-1} },
			.color = global_colors[1]
		};
		addWorldMesh(temp1);
	}

	POINT m_pos = { 0 };

	MSG msg;
	while (program->running) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				program->running = 0;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		program->input_tick++;
		if (program->input_tick > inputFreq) {
			if (GetAsyncKeyState(VK_LEFT) & 0x8000)
				camera->rot.f[0] -= 1;
			if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
				camera->rot.f[0] += 1;
			if (GetAsyncKeyState(VK_UP) & 0x8000)
				camera->rot.f[1] -= 1;
			if (GetAsyncKeyState(VK_DOWN) & 0x8000)
				camera->rot.f[1] += 1;

			if (GetAsyncKeyState(VK_SPACE) & 0x8000)
				camera->pos.f[1] -= 0.1;
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
				camera->pos.f[1] += 0.1;
			if (GetAsyncKeyState('W')) {
				camera->pos.f[0] += 0.1 * sin(camera->rot.f[0] * (3.14156 / 180));
				camera->pos.f[2] += 0.1 * cos(camera->rot.f[0] * (3.14156 / 180));
			}
			if (GetAsyncKeyState('S')) {
				camera->pos.f[0] -= 0.1 * sin(camera->rot.f[0] * (3.14156 / 180));
				camera->pos.f[2] -= 0.1 * cos(camera->rot.f[0] * (3.14156 / 180));
			}
			if (GetAsyncKeyState('D')) {
				camera->pos.f[0] += 0.1 * sin((camera->rot.f[0] + 90) * (3.14156 / 180));
				camera->pos.f[2] += 0.1 * cos((camera->rot.f[0] + 90) * (3.14156 / 180));
			}
			if (GetAsyncKeyState('A')) {
				camera->pos.f[0] += 0.1 * sin((camera->rot.f[0] - 90) * (3.14156 / 180));
				camera->pos.f[2] += 0.1 * cos((camera->rot.f[0] - 90) * (3.14156 / 180));
			}
			if (GetAsyncKeyState('1'))
				camera->freemode = 0;
			if (GetAsyncKeyState('2'))
				camera->freemode = 1;
			if (GetAsyncKeyState(VK_RETURN)) {
				initTerrain();
				terrainBuild();
			}
			program->input_tick = 0;

			if (program->m_active) {
				GetCursorPos(&m_pos);
				int dx = m_pos.x - program->res[0] / 2;
				int dy = m_pos.y - program->res[1] / 2;
				SetCursorPos(program->res[0] / 2, program->res[1] / 2);
				camera->rot.f[0] += dx * 0.25;
				camera->rot.f[1] += dy * 0.25;
			}
			if (GetAsyncKeyState('F'))
				camera->zoom += 1;
			if (GetAsyncKeyState('V'))
				camera->zoom -= 1;
			if (GetAsyncKeyState('R'))
				camera->zoom = 0;
		}

		program->draw_tick++;
		if (program->draw_tick > drawFreq) {
			//handle collision
			if (!camera->freemode) {
				for (int i = 0; i < max_world_meshes; i++) {
					if (!world_meshes[i]->color) continue;
					if (world_meshes[i]->verticies[0].f[0] < camera->pos.f[0] - 10 || (world_meshes[i]->verticies[0].f[0] > camera->pos.f[0] + 10))
						continue;
					if (world_meshes[i]->verticies[0].f[2] < camera->pos.f[2] - 10 || (world_meshes[i]->verticies[0].f[2] > camera->pos.f[2] + 10))
						continue;

					vec3 a = world_meshes[i]->verticies[0];
					vec3 b = world_meshes[i]->verticies[1];
					vec3 c = world_meshes[i]->verticies[2];

					if (pointInTriangleXZ(camera->pos, a, b, c)) {
						float y = barycentricY(camera->pos, a, b, c);
						camera->pos.f[1] = y - 12; // Now you have the "ground" height
						break;
					}
				}
			}
			//terrainWaves();
			InvalidateRect(window, NULL, TRUE);
			program->draw_tick = 0;
		}
	}
	return msg.wParam;
}

int cmp(const void* a, const void* b) {
	return ((visual_mesh*)b)->depth > ((visual_mesh*)a)->depth ? 1 : -1;
}

int compareVisualMeshes(const void* a, const void* b) {
	visual_mesh* meshA = *(visual_mesh**)a;
	visual_mesh* meshB = *(visual_mesh**)b;
	// Sort by depth descending (farthest first)
	if (meshA->depth < meshB->depth) return 1;
	if (meshA->depth > meshB->depth) return -1;
	return 0;
}

float clamp01(float x) {
	if (x < 0.0f) return 0.0f;
	if (x > 1.0f) return 1.0f;
	return x;
}

LRESULT CALLBACK proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
	static HBITMAP bitBuffer = NULL;
	static HDC hdcBuffer = NULL;
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
		if (wp == VK_ESCAPE)
			DestroyWindow(window);
		if (wp == VK_BACK) {
			program->m_active = !program->m_active;
			if (program->m_active)
				ShowCursor(FALSE);
			else
				ShowCursor(TRUE);
		}
		if (wp == '3')
			if (color_selection[0] < 6)
				color_selection[0]++;
		if (wp == '4')
			if (color_selection[0] > 0)
				color_selection[0]--;
		if (wp == '5')
			if (color_selection[1] < 6)
				color_selection[1]++;
		if (wp == '6')
			if (color_selection[1] > 0)
				color_selection[1]--;
		break;
	case WM_SIZE:
		if (program) {
			program->res[0] = LOWORD(lp);
			program->res[1] = HIWORD(lp);
		}
		break;
	case WM_PAINT: {
		if (!program) break;
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(window, &ps);
		//
		hdcBuffer = CreateCompatibleDC(hdc);
		bitBuffer = CreateCompatibleBitmap(hdc, program->res[0] / dscaling, program->res[1] / dscaling);
		SelectObject(hdcBuffer, bitBuffer);
		//
		RECT background;
		GetClientRect(window, &background);
		HBRUSH background_color = CreateSolidBrush(RGB(255,255,255));
		FillRect(hdcBuffer, &background, background_color);
		DeleteObject(background_color);

		HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
		SelectObject(hdcBuffer, pen);

		float cx = (program->res[0] / 2) / dscaling, cy = (program->res[1] / 2) / dscaling;

		if (program->debug_draw) {
			for (int i = 0; i < max_world_meshes; i++) {
				if (world_meshes[i]->color == NULL)
					continue;
				SelectObject(hdcBuffer, world_meshes[i]->color);
				for (int j = 0; j < 3; j++) {
					vec3 point = world_meshes[i]->verticies[j];
					
					for (int k = 0; k < 3; k++)
						point.f[k] -= camera->pos.f[k];
					vec2 t1 = rotate2d(point.f[0], point.f[2], camera->rot.f[0] * (3.14156 / 180));
					point.f[0] = t1.f[0]; point.f[2] = t1.f[1];
					vec2 t2 = rotate2d(point.f[1], point.f[2], camera->rot.f[1] * (3.14156 / 180));
					point.f[1] = t2.f[0]; point.f[2] = t2.f[1];

					float f = (cy + camera->zoom) / point.f[2];
					for (int k = 0; k < 2; k++)
						point.f[k] *= f;

					Ellipse(hdcBuffer, cx + point.f[0] - 4, cy + point.f[1] - 4, cx + point.f[0] + 4, cy + point.f[1] + 4);
				}
			}
		}
		
		//clear previous meshes
		removeAllVisualMeshes();
		for (int i = 0; i < max_world_meshes; i++) {
			if (world_meshes[i]->color == NULL)
				continue;

			if (program->culling) {
				//dont draw back faces
				// Get the transformed points (in camera/view space)
				vec3 p0 = world_meshes[i]->verticies[0];
				vec3 p1 = world_meshes[i]->verticies[1];
				vec3 p2 = world_meshes[i]->verticies[2];

				// Apply translation and rotation to camera space
				vec3 points_cam[3];
				for (int j = 0; j < 3; j++) {
					vec3 p = world_meshes[i]->verticies[j];
					// Translate
					for (int k = 0; k < 3; k++)
						p.f[k] -= camera->pos.f[k];
					// Rotate
					vec2 t1 = rotate2d(p.f[0], p.f[2], camera->rot.f[0] * (3.14156f / 180.0f));
					p.f[0] = t1.f[0]; p.f[2] = t1.f[1];
					vec2 t2 = rotate2d(p.f[1], p.f[2], camera->rot.f[1] * (3.14156f / 180.0f));
					p.f[1] = t2.f[0]; p.f[2] = t2.f[1];
					points_cam[j] = p;
				}

				// Compute the surface normal in camera space
				vec3 v1 = {
					points_cam[1].f[0] - points_cam[0].f[0],
					points_cam[1].f[1] - points_cam[0].f[1],
					points_cam[1].f[2] - points_cam[0].f[2]
				};
				vec3 v2 = {
					points_cam[2].f[0] - points_cam[0].f[0],
					points_cam[2].f[1] - points_cam[0].f[1],
					points_cam[2].f[2] - points_cam[0].f[2]
				};

				// Cross product to get normal
				vec3 normal = {
					v1.f[1] * v2.f[2] - v1.f[2] * v2.f[1],
					v1.f[2] * v2.f[0] - v1.f[0] * v2.f[2],
					v1.f[0] * v2.f[1] - v1.f[1] * v2.f[0]
				};

				// If Z component of normal is >= 0, it's facing away — skip it
				if (normal.f[2] <= 0.0f) {
					continue; // backface, don't draw
				}
			}

			visual_mesh temp_mesh = { 0 };
			int add_mesh = 1;
			int num_z_clips = 0;
			for (int j = 0; j < 3; j++) {
				vec3 point = world_meshes[i]->verticies[j];

				//apply transforms
				for (int k = 0; k < 3; k++)
					point.f[k] -= camera->pos.f[k];
				vec2 t1 = rotate2d(point.f[0], point.f[2], camera->rot.f[0] * (3.14156 / 180));
				point.f[0] = t1.f[0]; point.f[2] = t1.f[1];
				vec2 t2 = rotate2d(point.f[1], point.f[2], camera->rot.f[1] * (3.14156 / 180));
				point.f[1] = t2.f[0]; point.f[2] = t2.f[1];

				if (point.f[2] < 0) {
					if (point.f[2] < -2) {
						add_mesh = 0;
						continue;
					}
					point.f[2] = 0.1;
				}
				

				float f = (cy + camera->zoom) / point.f[2];
				point.f[0] *= f;
				point.f[1] *= f;
				/*
				for (int k = 0; k < 2; k++)
					point.f[k] *= f;	
				*/
				
				/*
				for (int k = 0; k < 2; k++) {
					if (point.f[2] > 20) { //ignore close triangles, they are already accounted for
						if (point.f[k] < -(program->res[k] / dscaling) / 2) {
							if (point.f[k] < -(program->res[k] / dscaling)) {
								add_mesh = 0;
								continue;
							}
							point.f[k] = -(program->res[k] / dscaling) / 2;
						}
						if (point.f[k] > (program->res[k] / dscaling) / 2) {
							if (point.f[k] > (program->res[k] / dscaling)) {
								add_mesh = 0;
								continue;
							}
							point.f[k] = (program->res[k] / dscaling) / 2;
						}
					}
				}
				*/

				for (int k = 0; k < 2; k++) {
					if (point.f[2] > 60) {
						if (point.f[k] < -(program->res[k] / dscaling) / 1.15 || point.f[k] > (program->res[k] / dscaling) / 1.15) {
							add_mesh = 0;
							continue;
						}
					}
				}
				/* - sin(camera->rot.f[1] * (3.14156 / 180)) * 50*/
				/*
				if (point.f[2] > draw_dist) {
					add_mesh = 0;
					continue;
				}
				*/
				{
					//float lol = point.f[2] + abs(point.f[0] * (point.f[2] * 0.15) * 0.1) / (1 + (camera->zoom * 0.01));
					float lol = point.f[2] + ((point.f[0] * point.f[0]) / 89999) * point.f[2] / (1 + (camera->zoom * 0.01));
					if (lol > draw_dist) {
						add_mesh = 0;
						continue;
					}
					temp_mesh.color_fade = lol;
				}

				temp_mesh.depth += point.f[2];
				temp_mesh.points[j] = (POINT){cx + point.f[0], cy + point.f[1]};
			}
			if (add_mesh) {
				//temp_mesh.color_fade /= 3;
				temp_mesh.final_color = world_meshes[i]->color;
				addVisualMesh(temp_mesh);
			}
		}
		{
			visual_mesh* activeMeshes[max_visual_meshes];
			int count = 0;

			// Collect active meshes
			for (int i = 0; i < max_visual_meshes; i++) {
				if (visual_meshes[i]->final_color != NULL) {
					activeMeshes[count++] = visual_meshes[i];
				}
			}

			// Sort active meshes by depth
			qsort(activeMeshes, count, sizeof(visual_mesh*), compareVisualMeshes);

			// Draw in sorted order
			for (int i = 0; i < count; i++) {
				//float lol = activeMeshes[i]->depth * 0.5;
				//float lol = activeMeshes[i]->color_fade * 1.25;
				float lol = 0;
				if (lol > 255)
					lol = 255;

				//SelectObject(hdcBuffer, GetStockObject(NULL_PEN));
				HBRUSH temp_color = CreateSolidBrush(RGB(lol, lol, lol));
				SelectObject(hdcBuffer, temp_color);
				Polygon(hdcBuffer, activeMeshes[i]->points, 3);
				DeleteObject(temp_color);
			}
		}

		StretchBlt(hdc, 0, 0, program->res[0], program->res[1], hdcBuffer, 0, 0, program->res[0] / dscaling, program->res[1] / dscaling, SRCCOPY);
		EndPaint(window, &ps);

		DeleteDC(hdcBuffer);
		DeleteObject(bitBuffer);
		DeleteObject(pen);
		break;
	}
	default:
		return DefWindowProc(window, msg, wp, lp);
	}
	return 0;
}