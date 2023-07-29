/** phiola: GUI: loader & worker
2023, Simon Zolin */

#include <gui/gui.h>
#include <FFOS/perf.h>

struct gui *gg;

void gui_dragdrop(ffstr data)
{
	ffvec fn = {};
	ffstr d = data;
	while (!ffui_fdrop_next(&fn, &d)) {
		list_add(*(ffstr*)&fn);
	}
	ffvec_free(&fn);
	ffstr_free(&data);
}

void gui_userconf_load()
{
	char *fn = ffsz_allocfmt("%s%s", gd->user_conf_dir, USER_CONF_NAME);
	ffvec buf = {};
	if (!!fffile_readwhole(fn, &buf, 1*1024*1024))
		goto end;

	ffstr d = FFSTR_INITSTR(&buf), line, k, v;
	while (d.len) {
		ffstr_splitby(&d, '\n', &line, &d);
		ffstr_splitby(&line, ' ', &k, &v);
		if (k.len)
			wmain_userconf_read(k, v);
	}

end:
	ffvec_free(&buf);
	ffmem_free(fn);
}

extern const ffui_ldr_ctl wmain_ctls[];

static void* gui_getctl(void *udata, const ffstr *name)
{
	static const ffui_ldr_ctl top_ctls[] = {
		FFUI_LDR_CTL(struct gui, mfile),
		FFUI_LDR_CTL(struct gui, mlist),
		FFUI_LDR_CTL(struct gui, mplay),
		FFUI_LDR_CTL3_PTR(struct gui, wmain, wmain_ctls),
		FFUI_LDR_CTL_END
	};

	struct gui *g = udata;
	return ffui_ldr_findctl(top_ctls, g, name);
}

static int gui_getcmd(void *udata, const ffstr *name)
{
	static const char action_str[][20] = {
		#define X(id)  #id
		#include "actions.h"
		#undef X
	};

	for (uint i = 0;  i != FF_COUNT(action_str);  i++) {
		if (ffstr_eqz(name, action_str[i]))
			return i + 1;
	}
	return 0;
}

static int load_ui()
{
	int r = -1;
	char *fn = ffsz_allocfmt("%Smod/gui/phiola.gui", &core->conf.root);
	ffui_loader ldr;
	ffui_ldr_init(&ldr, gui_getctl, gui_getcmd, gg);

	fftime t1;
	if (core->conf.log_level >= PHI_LOG_DEBUG)
		t1 = fftime_monotonic();

	if (!!ffui_ldr_loadfile(&ldr, fn)) {
		errlog("parsing phiola.gui: %s", ffui_ldr_errstr(&ldr));
		goto done;
	}
	r = 0;

done:
	if (core->conf.log_level >= PHI_LOG_DEBUG) {
		fftime t2 = fftime_monotonic();
		fftime_sub(&t2, &t1);
		dbglog("loaded GUI in %Ums", (int64)fftime_to_msec(&t2));
	}

	ffui_ldr_fin(&ldr);
	ffmem_free(fn);
	return r;
}

int FFTHREAD_PROCCALL gui_worker(void *param)
{
	gg = ffmem_new(struct gui);
	ffui_init();
	wmain_init();

	if (!!load_ui())
		goto end;
	gui_userconf_load();

	wmain_show();

	dbglog("entering GTK loop");
	ffui_run();
	dbglog("exited GTK loop");

end:
	ffui_uninit();
	gui_stop();
	return 0;
}

void gui_quit()
{
	gui_core_task(lists_save);

	ffvec buf = {};
	wmain_userconf_write(&buf);
	gui_core_task_data(userconf_save, *(ffstr*)&buf);

	ffui_post_quitloop();
}