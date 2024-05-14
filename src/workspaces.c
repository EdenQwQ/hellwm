#include <iso646.h>
#include <lua.h>
#include <time.h>
#include <stdio.h>
#include <wayland-cursor.h>
#include <wayland-util.h>
#include <wchar.h>
#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <complex.h>
#include <pthread.h>
#include <stdbool.h>
#include <wlr/backend.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_seat.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_keyboard.h>
#include <wayland-server-protocol.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_subcompositor.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>

#ifdef XWAYLAND
#include <xcb/xcb.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif

#include "../include/server.h"

/* TODO - add workspaces, !layout (reminder) */
/*	TODO: add borders */

struct hellwm_tile_tree *hellwm_tile_setup(struct wlr_output *output)
{
	struct hellwm_tile_tree *rsetup = malloc(sizeof(struct hellwm_tile_tree));

	if (rsetup == NULL)
	{
		hellwm_log(HELLWM_ERROR, "Failed to allocate memory for root inside hellwm_tile_setup().");
		return NULL;
	}

	rsetup->parent = NULL; /* This one NULL mean that the what ever branch is the root*/
	rsetup->left = NULL;
	rsetup->right = NULL;
	rsetup->x = 0;
	rsetup->y = 0;
	rsetup->direction = 0;
	rsetup->toplevel = NULL;

	rsetup->width = output->width;
	rsetup->height = output->height;

	return rsetup;
}

struct hellwm_tile_tree *hellwm_tile_farthest(struct hellwm_tile_tree *root, bool left, int i)
{
	i++;
	hellwm_log(HELLWM_LOG, "Inside hellwm_tile_farthest() - %d", i);
	if (root == NULL)
	{
		hellwm_log(HELLWM_ERROR, "Root is NULL inside hellwm_tile_farthest().");
		return NULL;
	}

	if (left)
	{
		if (root->left == NULL)
		{
			return root;
		}
		return hellwm_tile_farthest(root->left, left, i);
	}
	else
	{
		if (root->right == NULL)
		{
			return root;
		}
		return hellwm_tile_farthest(root->right, left, i);
	}
}

struct hellwm_tile_tree *hellwm_tile_find_toplevel(struct hellwm_tile_tree *node, struct hellwm_toplevel *toplevel)
{
	if (node->toplevel == toplevel)
	{
		return node;
	}
	if (node->left != NULL)
	{
		hellwm_tile_find_toplevel(node->left, toplevel);
	}
	if (node->right != NULL)
	{
		hellwm_tile_find_toplevel(node->right, toplevel);
	}
	return NULL;
}

void hellwm_tile_insert_toplevel(struct hellwm_tile_tree *node, struct hellwm_toplevel *new_toplevel, bool left)
{

	struct wlr_output *output = wlr_output_layout_get_center_output(new_toplevel->server->output_layout);
	
	if (node == NULL)
	{
		hellwm_log(HELLWM_ERROR, "Node is NULL inside hellwm_tile_insert_toplevel().");
		return;
	}

	struct hellwm_tile_tree *branch = malloc(sizeof(struct hellwm_tile_tree));

	if (branch == NULL)
	{
		hellwm_log(HELLWM_ERROR, "Failed to allocate memory for branch inside hellwm_tile_insert_toplevel().");
		return;
	}

	int new_width = node->width;
	int new_height = node->height;

	int direction = 0;

	int x = node->x;
	int y = node->y;
	
	if (node->parent != NULL)
	{
		if (node->direction == 0)
		{
			new_width = new_width/2;
			direction = 1;

			x = node->x+new_width;
			y = y;
		}
		else
		{
			new_height = new_height/2;
			direction = 0;

			x = x;
			y = node->y+new_height;
		}
		wlr_xdg_toplevel_set_size(node->toplevel->xdg_toplevel, new_width, new_height);
	}

	branch->parent = node;
	branch->left = NULL;
	branch->right = NULL;
	
	branch->width = new_width;
	branch->height = new_height;

	branch->x = x;
	branch->y = y;
	
	branch->direction = direction;
	branch->toplevel = new_toplevel;

	wlr_scene_node_set_position(&branch->toplevel->scene_tree->node, x, y);
	wlr_xdg_toplevel_set_size(new_toplevel->xdg_toplevel, new_width, new_height);

	hellwm_log(HELLWM_INFO, "Inserting toplevel at %d %d with %d %d", x, y, new_width, new_height);

	// for tests only - remove change later
	node->right = branch;
}

void hellwm_tile_recalculate_bellow(struct hellwm_tile_tree *node)
{
	if (node == NULL)
	{
		return;
	}
	if (node->left != NULL)
	{
		if (node->left->toplevel != NULL)
		{
			hellwm_tile_insert_toplevel(node, node->left->toplevel, false);
			hellwm_tile_recalculate_bellow(node);
		}
	}
	if (node->right != NULL)
	{
		if (node->right != NULL)
		{
			hellwm_tile_insert_toplevel(node, node->right->toplevel, false);
			hellwm_tile_recalculate_bellow(node);
		}
	}
}

void hellmw_tile_erase_toplevel(struct hellwm_toplevel *toplevel)
{
	if (toplevel == NULL)
		return;
	struct hellwm_tile_tree *node = hellwm_tile_find_toplevel(toplevel->server->tile_tree, toplevel);
	if (node == NULL)
		return;

	if (node->left != NULL)
	{
		if (node->left->toplevel != NULL)
			hellwm_tile_insert_toplevel(toplevel->server->tile_tree, node->left->toplevel, false);
	}

	if (node->right != NULL)
	{
		if (node->right->toplevel != NULL)
			hellwm_tile_insert_toplevel(toplevel->server->tile_tree, node->right->toplevel, false);
	}
	free(node);
}

void spiralTiling(struct hellwm_toplevel *new_toplevel)
{
	struct hellwm_server *server = new_toplevel->server;

   int direction = 0;
	
	struct wlr_output *output = wlr_output_layout_get_center_output(server->output_layout);
	int sWidth = output->width;
	int sHeight = output->height;

	int width = sWidth, height = sHeight;

	int n = wl_list_length(&server->toplevels);
	int length = wl_list_length(&server->toplevels);

	int x=0, y=0; 
	for (int i = 0; i < n+1; i++)
	{
		x = sWidth-width;
		y = sHeight-height;
	
		
		if (direction == 0)
		{
			width = width/2;
			direction = 1;
		}
		else
		{
			height = height/2;
			direction = 0;
		}
	}
		
	hellwm_log(HELLWM_LOG, "%d - Tiling: %d %d %d %d", n , x, y, width, height);
	wlr_scene_node_set_position(&new_toplevel->scene_tree->node, x, y);
	wlr_xdg_toplevel_set_size(new_toplevel->xdg_toplevel,  width,  height);

	hellwm_log("", "");
}
