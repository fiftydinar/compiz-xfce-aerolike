/*
 * Copyright © 2008 Dennis Kasprzyk
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Dennis Kasprzyk not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Dennis Kasprzyk makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DENNIS KASPRZYK DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Dennis Kasprzyk <onestone@compiz-fusion.org>
 *          David Reveman <davidr@novell.com>
 */

#ifndef _COMPOSITE_PRIVATES_H
#define _COMPOSITE_PRIVATES_H

#include <memory>
#include <boost/shared_ptr.hpp>

#include <composite/composite.h>
#include <core/atoms.h>
#include <map>
#include <vector>

#include "pixmapbinding.h"
#include "backbuffertracking.h"
#include "composite_options.h"

/*
 * One output that is actually scanning out a mode, with the interval that mode
 * runs at. Compiz has a single virtual framebuffer, but the X server it runs on
 * gives every monitor its own, so the interval to produce frames at depends on
 * which outputs need the next frame - see intervalForPendingDamage().
 */
struct OutputRate
{
    CompRect     geometry;
    int          rate;
    /* Exact frame interval from the pixel clock. The rate above is rounded to
     * whole hertz, which is what the log line and redrawTime use, but pacing on
     * a rounded value drifts: 1000 / 60 is 16ms against a 16.667ms frame, and
     * 1000 / 144 is 6ms against 6.944ms. */
    unsigned int intervalUs;
};

extern CompPlugin::VTable *compositeVTable;

extern CompWindow *lastDamagedWindow;

enum DamageTracking
{
    DamageForCurrentFrame = 0,
    DamageForLastFrame = 1,
    DamageFinalPaintRegion
};

class PrivateCompositeScreen :
    ScreenInterface,
    public CompositeOptions
{
    public:
	PrivateCompositeScreen (CompositeScreen *cs);
	~PrivateCompositeScreen ();

	bool setOption (const CompString &name, CompOption::Value &value);

	void outputChangeNotify ();

	void handleEvent (XEvent *event);

	void makeOutputWindow ();

	bool init ();

	void handleExposeEvent (XExposeEvent *event);

	void detectRefreshRate ();

	/* Shortest interval among the outputs the pending damage touches, in
	 * microseconds, or 0 when no output does. */
	unsigned int intervalForPendingDamage () const;

	void scheduleRepaint ();

	const CompRegion * damageTrackedBuffer (const CompRegion &);

    public:

	CompositeScreen *cScreen;

	int compositeEvent, compositeError, compositeOpcode;
	int damageEvent, damageError;
	int fixesEvent, fixesError, fixesVersion;

	bool shapeExtension;
	int  shapeEvent, shapeError;

	bool randrExtension;
	int  randrEvent, randrError;

	CompRegion    lastFrameDamage;
	unsigned long damageMask;

	CompRegion     tmpRegion;

	DamageTracking currentlyTrackingDamage;

	Window	      overlay;
	Window	      output;

	std::list <CompRect> exposeRects;

	CompPoint windowPaintOffset;

	int overlayWindowCount;
	bool outputShapeChanged;

	struct timeval lastRedraw;
	int            redrawTime;
	int            optimalRedrawTime;

	/* Enabled outputs and their intervals, refreshed on every RandR screen
	 * change. */
	std::vector<OutputRate> outputRates;

	/* Shortest interval of any enabled output, and the interval the queued frame
	 * was scheduled for. The latter is what the frame timing messages report,
	 * because it is the only rate the frame was actually paced to. */
	unsigned int           fastestIntervalUs;
	unsigned int           pacedIntervalUs;
	unsigned int           scheduledIntervalUs;

	/* Damage accumulated since the last repaint, reset when a frame starts.
	 * scheduleRepaint() paces to the fastest output this touches, so an update
	 * confined to a 60Hz monitor is produced at 60Hz instead of at the rate of
	 * some other monitor that is not changing. optimalRedrawTime stays at the
	 * fastest output overall, independent of what is damaged, because plugins
	 * read it as "the" frame interval for animation timing. */
	CompRegion pendingDamage;

	bool           scheduled, painting, reschedule;
	bool           damageRequiresRepaintReschedule;

	bool slowAnimations;

	CompTimer paintTimer;

	compiz::composite::PaintHandler *pHnd;

	CompositeFPSLimiterMode FPSLimiterMode;

	CompWindowList withDestroyedWindows;

	Atom cmSnAtom;
	Window newCmSnOwner;

	/* Map Damage handle to its bounding box */
	std::map<Damage, XRectangle> damages;

	compiz::composite::buffertracking::AgeingDamageBuffers ageingBuffers;
	compiz::composite::buffertracking::FrameRoster         roster;
};

class PrivateCompositeWindow :
    public WindowInterface,
    public CompositePixmapRebindInterface,
    public WindowPixmapGetInterface,
    public WindowAttributesGetInterface,
    public PixmapFreezerInterface
{
    public:
	PrivateCompositeWindow (CompWindow *w, CompositeWindow *cw);

	void windowNotify (CompWindowNotify n);
	void resizeNotify (int dx, int dy, int dwidth, int dheight);
	void moveNotify (int dx, int dy, bool now);

	Pixmap pixmap () const;
	bool   bind ();
	const CompSize & size () const;
	void release ();
	void setNewPixmapReadyCallback (const boost::function <void ()> &);
	void allowFurtherRebindAttempts ();
	bool frozen ();

	static void handleDamageRect (CompositeWindow *w, XRectangle const& rect);

    public:
	CompWindow      *window;
	CompositeWindow *cWindow;
	CompositeScreen *cScreen;

	PixmapBinding mPixmapBinding;

	Damage	      damage;

	bool	      damaged;
	bool	      redirected;
	bool          overlayWindow;

	unsigned short opacity;
	unsigned short brightness;
	unsigned short saturation;

	std::vector<XRectangle> damageRects;

    private:

	bool getAttributes (XWindowAttributes &);
	WindowPixmapInterface::Ptr getPixmap ();
};

#endif
