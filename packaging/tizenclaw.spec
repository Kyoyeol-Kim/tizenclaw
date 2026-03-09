Name:       tizenclaw
Summary:    TizenClaw Agent System Service App
Version:    1.0.0
Release:    1
Group:      System/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: %{name}.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(tizen-core)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(gtest)
BuildRequires:  pkgconfig(gmock)
BuildRequires:  pkgconfig(libsoup-2.4)
BuildRequires:  pkgconfig(libwebsockets)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(capi-appfw-tizen-action)

%description
TizenClaw Native Agent running as a System Service, utilizing LXC for skills execution.

%package unittests
Summary: Unit tests for TizenClaw
Group: System/Service
Requires: %{name} = %{version}-%{release}

%description unittests
Unit tests for TizenClaw

%prep
%setup -q -n %{name}-%{version}
cp %{SOURCE1001} .

%build
export CFLAGS="$CFLAGS -Wall -Wno-shadow -Wno-unused-function"
export CXXFLAGS="$CXXFLAGS -Wall"
export LDFLAGS="$LDFLAGS -Wl,--as-needed"

%cmake . -DTIZENCLAW_ARCH=%{_arch}
%__make %{?_smp_mflags}

# Build crun from source
CRUN_VERSION="1.26"
CRUN_TAR="third_party/crun/src/crun-${CRUN_VERSION}.tar.gz"
CRUN_STAGE_DIR="crun-stage"
rm -rf "${CRUN_STAGE_DIR}"
mkdir -p "${CRUN_STAGE_DIR}"

if [ -f "${CRUN_TAR}" ]; then
  tar -xf "${CRUN_TAR}" -C "${CRUN_STAGE_DIR}"
  CRUN_SRC_DIR="${CRUN_STAGE_DIR}/crun-${CRUN_VERSION}"
  if [ ! -x "${CRUN_SRC_DIR}/configure" ] && [ -x "${CRUN_SRC_DIR}/autogen.sh" ]; then
    (cd "${CRUN_SRC_DIR}" && ./autogen.sh)
  fi

  # Override CFLAGS for third-party crun build to suppress warnings
  # that become errors under -Werror (shadow, unused-variable, stringop).
  export CFLAGS="${CFLAGS} -Wno-shadow -Wno-unused-variable -Wno-stringop-overread -Wno-stringop-overflow"

  if (cd "${CRUN_SRC_DIR}" && \
      ./configure \
        --prefix=/usr \
        --disable-systemd \
        --disable-shared \
        --enable-embedded-yajl \
        --disable-caps \
        --disable-seccomp && \
      make %{?_smp_mflags}); then
    cp "${CRUN_SRC_DIR}/crun" "${CRUN_STAGE_DIR}/crun"
    chmod +x "${CRUN_STAGE_DIR}/crun"
    echo "Built crun from source successfully."
  else
    echo "ERROR: Source build for crun failed."
    exit 1
  fi
else
  echo "ERROR: Missing crun source tarball: ${CRUN_TAR}"
  exit 1
fi

%check
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:%{buildroot}/usr/lib
ctest -V

%install
%make_install

CRUN_SRC="crun-stage/crun"
if [ ! -x "${CRUN_SRC}" ]; then
  echo "Missing source-built crun binary: ${CRUN_SRC}"
  echo "crun source build must succeed before install."
  exit 1
fi
echo "Using source-built crun binary: ${CRUN_SRC}"
install -D -m 0755 "${CRUN_SRC}" \
  %{buildroot}/usr/libexec/tizenclaw/crun

# Tizen structure
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_unitdir}
mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
mkdir -p %{buildroot}/opt/usr/share/tizenclaw/img
mkdir -p %{buildroot}/opt/usr/share/tizenclaw/skills
mkdir -p %{buildroot}/opt/usr/share/tizenclaw/config
mkdir -p %{buildroot}/opt/usr/share/tizenclaw/tools/embedded

ln -sf ../tizenclaw.service %{buildroot}%{_unitdir}/multi-user.target.wants/tizenclaw.service
ln -sf ../tizenclaw-skills-secure.service %{buildroot}%{_unitdir}/multi-user.target.wants/tizenclaw-skills-secure.service

%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%{_bindir}/tizenclaw
%{_bindir}/tizenclaw-cli
%{_bindir}/start_mcp_tunnel.sh
%{_unitdir}/tizenclaw.service
%{_unitdir}/tizenclaw-skills-secure.service
%{_unitdir}/multi-user.target.wants/tizenclaw.service
%{_unitdir}/multi-user.target.wants/tizenclaw-skills-secure.service
/usr/libexec/tizenclaw/run_standard_container.sh
/usr/libexec/tizenclaw/skills_secure_container.sh
/usr/libexec/tizenclaw/crun
/opt/usr/share/tizenclaw/img/rootfs.tar.gz
/opt/usr/share/tizenclaw/config/*
/opt/usr/share/tizenclaw/skills/
/opt/usr/share/tizenclaw/web/
/opt/usr/share/tizenclaw/tools/embedded/
%dir /opt/usr/share/tizenclaw/tools/
%dir /opt/usr/share/tizenclaw/config/
%dir /opt/usr/share/tizenclaw/

%files unittests
%defattr(-,root,root,-)
%manifest %{name}.manifest
%{_bindir}/tizenclaw-unittests
