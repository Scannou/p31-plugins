//
//  ObjCNames.h
//  p31-render
//
//  Objective-C classes share one namespace per process, and a host loads
//  several P31 plugins (and sometimes several formats or versions of one)
//  side by side. Two bundles defining the same class name would make the
//  runtime pick one implementation for both. Every class here therefore gets
//  a per-plugin, per-version suffix (P31_OBJC_SUFFIX, set by CMake), and the
//  render sources are compiled into each plugin separately.
//
//  Code uses the plain names below; the macros expand them.
//

#pragma once

#ifndef P31_OBJC_SUFFIX
#error "P31_OBJC_SUFFIX must be defined by the build"
#endif

#define P31_OBJC_CAT2(a, b) a##_##b
#define P31_OBJC_CAT(a, b) P31_OBJC_CAT2(a, b)
#define P31_OBJC_CLASS(name) P31_OBJC_CAT(name, P31_OBJC_SUFFIX)

#define P31AnimatedView       P31_OBJC_CLASS(P31AnimatedView)
#define P31DisplayLinkTarget  P31_OBJC_CLASS(P31DisplayLinkTarget)
#define P31ScopeView          P31_OBJC_CLASS(P31ScopeView)
#define P31SpectrumView       P31_OBJC_CLASS(P31SpectrumView)
#define P31SettingsPanel      P31_OBJC_CLASS(P31SettingsPanel)
#define P31FlippedView        P31_OBJC_CLASS(P31FlippedView)
#define P31MenuItem           P31_OBJC_CLASS(P31MenuItem)
#define P31EditorRootView     P31_OBJC_CLASS(P31EditorRootView)
