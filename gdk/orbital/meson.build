gdk_orbital_sources = files(
  'gdkapplaunchcontext-orbital.c',
  'gdkcursor-orbital.c',
  'gdkdevice-orbital.c',
  'gdkdisplay-orbital.c',
  'gdkdnd-orbital.c',
  'gdkeventsource.c',
  'gdkkeys-orbital.c',
  'gdkglcontext-orbital.c',
  'gdkmonitor-orbital.c',
  'gdkscreen-orbital.c',
  'gdkselection-orbital.c',
  'gdkwindow-orbital.c',
  'wm-button-layout-translation.c',
)

gdk_orbital_public_headers = files(
  'gdkorbitaldevice.h',
  'gdkorbitaldisplay.h',
  'gdkorbitalglcontext.h',
  'gdkorbitalmonitor.h',
  'gdkorbitalselection.h',
  'gdkorbitalwindow.h',
)

install_headers(gdk_orbital_public_headers, subdir: 'gtk-3.0/gdk/orbital')
install_headers('gdkorbital.h', subdir: 'gtk-3.0/gdk')

gdk_orbital_deps = [
  shmlib,
  xkbdep,
  orbitaldep,
]

libgdk_orbital = static_library('gdk-orbital',
                                gdk_orbital_sources, gdkconfig, gdkenum_h,
                                include_directories: [ confinc, gdkinc, ],
                                c_args: [
                                  '-DGDK_COMPILATION',
                                  '-DG_LOG_DOMAIN="Gdk"',
                                ] + common_cflags,
                                link_args: common_ldflags,
                                dependencies: [ gdk_deps, gdk_orbital_deps, ])
