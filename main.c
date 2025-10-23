#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include "display.h"
#include "vector.h"
#include "mesh.h"
#include "array.h"
#include "matrix.h"

bool backface_culling = true;

triangle_t *triangles_to_render = NULL;

vec3_t camera_position = { .x=0, .y=0, .z=0 };
mat4_t proj_matrix;

bool is_running = false;
int previous_frame_time = 0;


void setup(void) {

	render_method = RENDER_WIRE;
	cull_method = CULL_BACKFACE;

	// Allocate memory to hold the color buffer
	color_buffer = (uint32_t*) malloc(sizeof(uint32_t) * window_width * window_height);

	// Create an SDL texture to display the color buffer
	color_buffer_texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		window_width,
		window_height
	);

	//initialize perspective projection matrix

	float fov = M_PI / 3.0;
	float aspect_ratio = (float) window_width / (float) window_height;
	float znear = 0.1;
	float zfar = 100.0;

	proj_matrix = mat4_make_perspective(fov, aspect_ratio, znear, zfar);

	//load_obj_file_data("../Models/cube_triangulated.obj");
	load_cube_mesh_data();

}


void process_input(void) {

	SDL_Event event;
	SDL_PollEvent(&event);

	switch (event.type) {
		case SDL_QUIT:
			is_running = false;
			break;
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE)
				is_running = false;

			if (event.key.keysym.sym == SDLK_1)
				render_method = RENDER_WIRE_VERTEX;
			if (event.key.keysym.sym == SDLK_2)
				render_method = RENDER_WIRE;
			if (event.key.keysym.sym == SDLK_3)
				render_method = RENDER_FILL_TRIANGLE;
			if (event.key.keysym.sym == SDLK_4)
				render_method = RENDER_FILL_TRIANGLE_WIRE;
			if (event.key.keysym.sym == SDLK_c)
				cull_method = CULL_BACKFACE;
			if (event.key.keysym.sym == SDLK_d)
				cull_method = CULL_NONE;
			break;
	}

}


void update(void) {
	
	int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - previous_frame_time); //FTT 33 ms

	if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
		SDL_Delay(time_to_wait);
	}

	previous_frame_time = SDL_GetTicks();

	triangles_to_render = NULL;

	mesh.rotation.x += 0.005;
	//mesh.rotation.y += 0.005;
	//mesh.rotation.z += 0.005;

	//mesh.scale.x += 0.002;
	//mesh.scale.y += 0.002;
	//mesh.scale.z += 0.002;

	//mesh.translation.x += 0.002;
	//mesh.translation.x += 0.01;
	mesh.translation.z = 5.0;

	mat4_t scale_matrix = mat4_make_scale(mesh.scale.x, mesh.scale.y, mesh.scale.z);

	mat4_t translation_matrix = mat4_make_translation(mesh.translation.x, mesh.translation.y, mesh.translation.z);

	mat4_t rotation_matrix_x = mat4_make_rotation_x(mesh.rotation.x);
	mat4_t rotation_matrix_y = mat4_make_rotation_y(mesh.rotation.y);
	mat4_t rotation_matrix_z = mat4_make_rotation_z(mesh.rotation.z);

	int num_faces = array_length(mesh.faces);

	for (int i = 0; i < num_faces; i++) {
		face_t mesh_face = mesh.faces[i];

		vec3_t face_vertices[3];
		face_vertices[0] = mesh.vertices[mesh_face.a - 1];
		face_vertices[1] = mesh.vertices[mesh_face.b - 1];
		face_vertices[2] = mesh.vertices[mesh_face.c - 1];

		vec4_t transformed_vertices[3];

		for (int j = 0; j < 3; j++) {

			vec4_t transformed_vertex = vec4_from_vec3(face_vertices[j]);

			mat4_t world_matrix = mat4_identity();

			world_matrix = mat4_mul_mat4(scale_matrix, world_matrix);
			world_matrix = mat4_mul_mat4(rotation_matrix_x, world_matrix);
			world_matrix = mat4_mul_mat4(rotation_matrix_y, world_matrix);
			world_matrix = mat4_mul_mat4(rotation_matrix_z, world_matrix);
			world_matrix = mat4_mul_mat4(translation_matrix, world_matrix);

			transformed_vertex = mat4_mul_vec4(world_matrix, transformed_vertex);

			transformed_vertices[j] = transformed_vertex;

		}

		if (cull_method == CULL_BACKFACE) {


			vec3_t vector_a = vec3_from_vec4(transformed_vertices[0]);
			vec3_t vector_b = vec3_from_vec4(transformed_vertices[1]);
			vec3_t vector_c = vec3_from_vec4(transformed_vertices[2]);

			vec3_t vector_ab = vec3_sub(vector_b, vector_a);
			vec3_t vector_ac = vec3_sub(vector_c, vector_a);

			vec3_normalize(&vector_ab);
			vec3_normalize(&vector_ac);

			vec3_t normal = vec3_cross(vector_ab, vector_ac);
			vec3_normalize(&normal);

			vec3_t camera_ray = vec3_sub(camera_position, vector_a);

			float dot_normal_camera = vec3_dot(normal, camera_ray);

			if (dot_normal_camera < 0) {
				continue;
			}

		}
		
		vec4_t projected_points[3];

		for (int j = 0; j < 3; j++) {

			projected_points[j] = mat4_mul_vec4_project(proj_matrix, transformed_vertices[j]);

			projected_points[j].x *= (window_width / 2.0);
			projected_points[j].y *= (window_height / 2.0);

			projected_points[j].x += (window_width  / 2.0);
			projected_points[j].y += (window_height / 2.0);


		}

		float avg_depth = (transformed_vertices[0].z + transformed_vertices[1].z + transformed_vertices[2].z) / 3.0;

		triangle_t projected_triangle = {
			.points = {
				{ projected_points[0].x, projected_points[0].y },
				{ projected_points[1].x, projected_points[1].y },
				{ projected_points[2].x, projected_points[2].y },
			},
			.color = mesh_face.color,
			.avg_depth = avg_depth

		};

		array_push(triangles_to_render, projected_triangle);

	}

	int num_triangles = array_length(triangles_to_render);

	for (int i = 0; i < num_triangles; i++) {

		for (int j = i; j < num_triangles; j++) {

			if (triangles_to_render[i].avg_depth < triangles_to_render[j].avg_depth) {

				triangle_t temp = triangles_to_render[i];
				triangles_to_render[i] = triangles_to_render[j];
				triangles_to_render[j] = temp;

			}

		}

	}

}


void render(void) {

	//SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
	//SDL_RenderClear(renderer);
	
	
	int num_triangles = array_length(triangles_to_render);

	for (int i = 0; i < num_triangles; i++) {

		triangle_t triangle = triangles_to_render[i];



		if (render_method == RENDER_FILL_TRIANGLE || render_method == RENDER_FILL_TRIANGLE_WIRE) {

			draw_filled_triangle(
				triangle.points[0].x, triangle.points[0].y,
				triangle.points[1].x, triangle.points[1].y,
				triangle.points[2].x, triangle.points[2].y,
				triangle.color
			);

		}

		if (render_method == RENDER_WIRE || render_method == RENDER_WIRE_VERTEX || render_method == RENDER_FILL_TRIANGLE) {

			draw_triangle(
				triangle.points[0].x, triangle.points[0].y,
				triangle.points[1].x, triangle.points[1].y,
				triangle.points[2].x, triangle.points[2].y,
				0xFF808080
			);

		}

		if (render_method == RENDER_WIRE_VERTEX) {

			draw_rect(triangle.points[0].x - 3, triangle.points[0].y - 3, 6, 6, 0xFFFFF00);
			draw_rect(triangle.points[1].x - 3, triangle.points[1].y - 3, 6, 6, 0xFFFFF00);
			draw_rect(triangle.points[2].x - 3, triangle.points[2].y - 3, 6, 6, 0xFFFFF00);

		}


	}
	
	array_free(triangles_to_render);

	render_color_buffer();
	clear_color_buffer(0xFF000000);
	SDL_RenderPresent(renderer);

}

void free_resources() {

	array_free(mesh.faces);
	array_free(mesh.vertices);
	free(color_buffer);

}

int main(int argc, char* args[]) {

	// Create an SDL window 
	is_running = initialize_window();

	setup();
	
	while (is_running) {

		process_input();
		update();
		render();

	}

	destroy_window();
	free_resources();

	return 0;

}



