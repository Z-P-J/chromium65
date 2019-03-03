/* Generated by wayland-scanner 1.13.0 */

#ifndef AURA_SHELL_CLIENT_PROTOCOL_H
#define AURA_SHELL_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @page page_aura_shell The aura_shell protocol
 * @section page_ifaces_aura_shell Interfaces
 * - @subpage page_iface_zaura_shell - aura_shell
 * - @subpage page_iface_zaura_surface - aura shell interface to a wl_surface
 * - @subpage page_iface_zaura_output - aura shell interface to a wl_output
 * @section page_copyright_aura_shell Copyright
 * <pre>
 *
 * Copyright 2017 The Chromium Authors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * </pre>
 */
struct wl_output;
struct wl_surface;
struct zaura_output;
struct zaura_shell;
struct zaura_surface;

/**
 * @page page_iface_zaura_shell zaura_shell
 * @section page_iface_zaura_shell_desc Description
 *
 * The global interface exposing aura shell capabilities is used to
 * instantiate an interface extension for a wl_surface object.
 * This extended interface will then allow the client to use aura shell
 * specific functionality.
 * @section page_iface_zaura_shell_api API
 * See @ref iface_zaura_shell.
 */
/**
 * @defgroup iface_zaura_shell The zaura_shell interface
 *
 * The global interface exposing aura shell capabilities is used to
 * instantiate an interface extension for a wl_surface object.
 * This extended interface will then allow the client to use aura shell
 * specific functionality.
 */
extern const struct wl_interface zaura_shell_interface;
/**
 * @page page_iface_zaura_surface zaura_surface
 * @section page_iface_zaura_surface_desc Description
 *
 * An additional interface to a wl_surface object, which allows the
 * client to access aura shell specific functionality for surface.
 * @section page_iface_zaura_surface_api API
 * See @ref iface_zaura_surface.
 */
/**
 * @defgroup iface_zaura_surface The zaura_surface interface
 *
 * An additional interface to a wl_surface object, which allows the
 * client to access aura shell specific functionality for surface.
 */
extern const struct wl_interface zaura_surface_interface;
/**
 * @page page_iface_zaura_output zaura_output
 * @section page_iface_zaura_output_desc Description
 *
 * An additional interface to a wl_output object, which allows the
 * client to access aura shell specific functionality for output.
 * @section page_iface_zaura_output_api API
 * See @ref iface_zaura_output.
 */
/**
 * @defgroup iface_zaura_output The zaura_output interface
 *
 * An additional interface to a wl_output object, which allows the
 * client to access aura shell specific functionality for output.
 */
extern const struct wl_interface zaura_output_interface;

#ifndef ZAURA_SHELL_ERROR_ENUM
#define ZAURA_SHELL_ERROR_ENUM
enum zaura_shell_error {
	/**
	 * the surface already has an aura surface object associated
	 */
	ZAURA_SHELL_ERROR_AURA_SURFACE_EXISTS = 0,
	/**
	 * the output already has an aura output object associated
	 */
	ZAURA_SHELL_ERROR_AURA_OUTPUT_EXISTS = 1,
};
#endif /* ZAURA_SHELL_ERROR_ENUM */

#define ZAURA_SHELL_GET_AURA_SURFACE 0
#define ZAURA_SHELL_GET_AURA_OUTPUT 1


/**
 * @ingroup iface_zaura_shell
 */
#define ZAURA_SHELL_GET_AURA_SURFACE_SINCE_VERSION 1
/**
 * @ingroup iface_zaura_shell
 */
#define ZAURA_SHELL_GET_AURA_OUTPUT_SINCE_VERSION 2

/** @ingroup iface_zaura_shell */
static inline void
zaura_shell_set_user_data(struct zaura_shell *zaura_shell, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zaura_shell, user_data);
}

/** @ingroup iface_zaura_shell */
static inline void *
zaura_shell_get_user_data(struct zaura_shell *zaura_shell)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zaura_shell);
}

static inline uint32_t
zaura_shell_get_version(struct zaura_shell *zaura_shell)
{
	return wl_proxy_get_version((struct wl_proxy *) zaura_shell);
}

/** @ingroup iface_zaura_shell */
static inline void
zaura_shell_destroy(struct zaura_shell *zaura_shell)
{
	wl_proxy_destroy((struct wl_proxy *) zaura_shell);
}

/**
 * @ingroup iface_zaura_shell
 *
 * Instantiate an interface extension for the given wl_surface to
 * provide aura shell functionality. If the given wl_surface is not
 * associated with a shell surface, the shell_surface_missing protocol
 * error is raised.
 */
static inline struct zaura_surface *
zaura_shell_get_aura_surface(struct zaura_shell *zaura_shell, struct wl_surface *surface)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zaura_shell,
			 ZAURA_SHELL_GET_AURA_SURFACE, &zaura_surface_interface, NULL, surface);

	return (struct zaura_surface *) id;
}

/**
 * @ingroup iface_zaura_shell
 *
 * Instantiate an interface extension for the given wl_output to
 * provide aura shell functionality.
 */
static inline struct zaura_output *
zaura_shell_get_aura_output(struct zaura_shell *zaura_shell, struct wl_output *output)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zaura_shell,
			 ZAURA_SHELL_GET_AURA_OUTPUT, &zaura_output_interface, NULL, output);

	return (struct zaura_output *) id;
}

#ifndef ZAURA_SURFACE_FRAME_TYPE_ENUM
#define ZAURA_SURFACE_FRAME_TYPE_ENUM
/**
 * @ingroup iface_zaura_surface
 * different frame types
 *
 * Frame types that can be used to decorate a surface.
 */
enum zaura_surface_frame_type {
	/**
	 * no frame
	 */
	ZAURA_SURFACE_FRAME_TYPE_NONE = 0,
	/**
	 * caption with shadow
	 */
	ZAURA_SURFACE_FRAME_TYPE_NORMAL = 1,
	/**
	 * shadow only
	 */
	ZAURA_SURFACE_FRAME_TYPE_SHADOW = 2,
};
#endif /* ZAURA_SURFACE_FRAME_TYPE_ENUM */

#define ZAURA_SURFACE_SET_FRAME 0
#define ZAURA_SURFACE_SET_PARENT 1
#define ZAURA_SURFACE_SET_FRAME_COLORS 2


/**
 * @ingroup iface_zaura_surface
 */
#define ZAURA_SURFACE_SET_FRAME_SINCE_VERSION 1
/**
 * @ingroup iface_zaura_surface
 */
#define ZAURA_SURFACE_SET_PARENT_SINCE_VERSION 2
/**
 * @ingroup iface_zaura_surface
 */
#define ZAURA_SURFACE_SET_FRAME_COLORS_SINCE_VERSION 3

/** @ingroup iface_zaura_surface */
static inline void
zaura_surface_set_user_data(struct zaura_surface *zaura_surface, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zaura_surface, user_data);
}

/** @ingroup iface_zaura_surface */
static inline void *
zaura_surface_get_user_data(struct zaura_surface *zaura_surface)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zaura_surface);
}

static inline uint32_t
zaura_surface_get_version(struct zaura_surface *zaura_surface)
{
	return wl_proxy_get_version((struct wl_proxy *) zaura_surface);
}

/** @ingroup iface_zaura_surface */
static inline void
zaura_surface_destroy(struct zaura_surface *zaura_surface)
{
	wl_proxy_destroy((struct wl_proxy *) zaura_surface);
}

/**
 * @ingroup iface_zaura_surface
 *
 * Suggests a surface should use a specific frame.
 */
static inline void
zaura_surface_set_frame(struct zaura_surface *zaura_surface, uint32_t type)
{
	wl_proxy_marshal((struct wl_proxy *) zaura_surface,
			 ZAURA_SURFACE_SET_FRAME, type);
}

/**
 * @ingroup iface_zaura_surface
 *
 * Set the "parent" of this surface. "x" and "y" arguments specify the
 * initial position for surface relative to parent.
 */
static inline void
zaura_surface_set_parent(struct zaura_surface *zaura_surface, struct zaura_surface *parent, int32_t x, int32_t y)
{
	wl_proxy_marshal((struct wl_proxy *) zaura_surface,
			 ZAURA_SURFACE_SET_PARENT, parent, x, y);
}

/**
 * @ingroup iface_zaura_surface
 *
 * Set the frame colors.
 */
static inline void
zaura_surface_set_frame_colors(struct zaura_surface *zaura_surface, uint32_t active_color, uint32_t inactive_color)
{
	wl_proxy_marshal((struct wl_proxy *) zaura_surface,
			 ZAURA_SURFACE_SET_FRAME_COLORS, active_color, inactive_color);
}

#ifndef ZAURA_OUTPUT_SCALE_PROPERTY_ENUM
#define ZAURA_OUTPUT_SCALE_PROPERTY_ENUM
/**
 * @ingroup iface_zaura_output
 * scale information
 *
 * These flags describe properties of an output scale.
 * They are used in the flags bitfield of the scale event.
 */
enum zaura_output_scale_property {
	/**
	 * indicates this is the current scale
	 */
	ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT = 0x1,
	/**
	 * indicates this is the preferred scale
	 */
	ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED = 0x2,
};
#endif /* ZAURA_OUTPUT_SCALE_PROPERTY_ENUM */

#ifndef ZAURA_OUTPUT_SCALE_FACTOR_ENUM
#define ZAURA_OUTPUT_SCALE_FACTOR_ENUM
enum zaura_output_scale_factor {
	ZAURA_OUTPUT_SCALE_FACTOR_0500 = 500,
	ZAURA_OUTPUT_SCALE_FACTOR_0600 = 600,
	ZAURA_OUTPUT_SCALE_FACTOR_0625 = 625,
	ZAURA_OUTPUT_SCALE_FACTOR_0750 = 750,
	ZAURA_OUTPUT_SCALE_FACTOR_0800 = 800,
	ZAURA_OUTPUT_SCALE_FACTOR_1000 = 1000,
	ZAURA_OUTPUT_SCALE_FACTOR_1125 = 1125,
	ZAURA_OUTPUT_SCALE_FACTOR_1200 = 1200,
	ZAURA_OUTPUT_SCALE_FACTOR_1250 = 1250,
	ZAURA_OUTPUT_SCALE_FACTOR_1500 = 1500,
	ZAURA_OUTPUT_SCALE_FACTOR_1600 = 1600,
	ZAURA_OUTPUT_SCALE_FACTOR_2000 = 2000,
};
#endif /* ZAURA_OUTPUT_SCALE_FACTOR_ENUM */

/**
 * @ingroup iface_zaura_output
 * @struct zaura_output_listener
 */
struct zaura_output_listener {
	/**
	 * advertise available scales for the output
	 *
	 * The scale event describes an available scale for the output.
	 *
	 * The event is sent when binding to the output object and there
	 * will always be one scale, the current scale. The event is sent
	 * again if an output changes scale, for the scale that is now
	 * current. In other words, the current scale is always the last
	 * scale that was received with the current flag set.
	 * @param flags bitfield of scale flags
	 * @param scale output scale
	 */
	void (*scale)(void *data,
		      struct zaura_output *zaura_output,
		      uint32_t flags,
		      uint32_t scale);
};

/**
 * @ingroup iface_zaura_output
 */
static inline int
zaura_output_add_listener(struct zaura_output *zaura_output,
			  const struct zaura_output_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zaura_output,
				     (void (**)(void)) listener, data);
}

/**
 * @ingroup iface_zaura_output
 */
#define ZAURA_OUTPUT_SCALE_SINCE_VERSION 1


/** @ingroup iface_zaura_output */
static inline void
zaura_output_set_user_data(struct zaura_output *zaura_output, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zaura_output, user_data);
}

/** @ingroup iface_zaura_output */
static inline void *
zaura_output_get_user_data(struct zaura_output *zaura_output)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zaura_output);
}

static inline uint32_t
zaura_output_get_version(struct zaura_output *zaura_output)
{
	return wl_proxy_get_version((struct wl_proxy *) zaura_output);
}

/** @ingroup iface_zaura_output */
static inline void
zaura_output_destroy(struct zaura_output *zaura_output)
{
	wl_proxy_destroy((struct wl_proxy *) zaura_output);
}

#ifdef  __cplusplus
}
#endif

#endif
