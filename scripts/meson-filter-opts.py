#!/usr/bin/env python3
"""Print meson arguments, dropping -Dname=value options the project doesn't define.

Upstream projects rename and remove build options between releases; an unknown -D makes meson
refuse to configure. Dropped options are reported on stderr so the recipe can be tidied later.
"""
import os, re, sys
names = set()
for f in ('meson.options', 'meson_options.txt'):
    if os.path.exists(f):
        names |= set(re.findall(r"option\s*\(\s*['\"]([^'\"]+)['\"]", open(f).read()))
builtin_prefix = ('b_', 'c_', 'cpp_', 'rust_', 'objc_')
builtin = {'default_library', 'backend', 'buildtype', 'debug', 'optimization', 'strip', 'warning_level', 'werror',
           'wrap_mode', 'auto_features', 'prefer_static', 'pkg_config_path', 'cmake_prefix_path', 'unity', 'layout',
           'install_umask', 'python.install_env', 'python.platlibdir', 'python.purelibdir', 'force_fallback_for'}
out = []
for a in sys.argv[1:]:
    m = re.match(r'-D([^=]+)=', a)
    if m and names:
        n = m.group(1)
        if n not in names and n not in builtin and not n.startswith(builtin_prefix) and ':' not in n:
            print(f'meson-filter-opts: dropping unknown option {a}', file=sys.stderr)
            continue
    out.append(a)
print('\0'.join(out), end='')
