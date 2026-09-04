%global commit0 0b8ed82
%global date 2026-09-04

Name:           praxis
Version:        0.1.0
Release:        1%{?dist}
Summary:        C++23 agent execution engine
License:        MIT
URL:            https://github.com/yuw868349-commits/praxis
Source0:        https://github.com/yuw868349-commits/praxis/archive/v%{version}.tar.gz

BuildRequires:  cmake >= 3.24
BuildRequires:  ninja-build
BuildRequires:  gcc-c++ >= 13
BuildRequires:  python3-devel
Requires:       libstdc++ >= 12

%description
Praxis runs plan-act-reflect multi-turn tasks with a built-in
context manager, tool executor, cache, replay, and telemetry. It
speaks MCP (stdio + SSE) to attach external tools and ships with
a Python SDK (pybind11).

%package        devel
Summary:        Development files for praxis
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
Headers and pkg-config file for building against praxis.

%prep
%autosetup -n praxis-%{commit0}

%build
%cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DPRAXIS_BUILD_TESTS=OFF
%cmake_build

%install
%cmake_install
mkdir -p %{buildroot}%{_pkgdocdir}
cp LICENSE %{buildroot}%{_pkgdocdir}/

%files
%license LICENSE
%doc README.md
%{_bindir}/praxis
%{_libdir}/libpraxis_core.a
%{_libdir}/libpraxis_tools.a
%{_libdir}/libpraxis_llm.a

%files devel
%{_includedir}/praxis/
%{_libdir}/pkgconfig/praxis.pc

%changelog
* %{date} Praxis Contributors <noreply@example.com> - 0.1.0-1
- Initial package
