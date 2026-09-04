class Praxis < Formula
  desc "C++23 agent execution engine"
  homepage "https://github.com/yuw868349-commits/praxis"
  url "https://github.com/yuw868349-commits/praxis/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  license "MIT"
  head "https://github.com/yuw868349-commits/praxis.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "python@3.12" => :build
  depends_on "pybind11" => :build

  def install
    system "cmake", "-S", ".", "-B", "build",
                    "-G", "Ninja",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DPRAXIS_BUILD_TESTS=OFF",
                    *std_cmake_args
    system "cmake", "--build", "build", "-j"
    system "cmake", "--install", "build"

    (pkgshare/"examples").install Dir["examples/cpp/*"]
    pkgshare.install "README.md", "LICENSE", "CHANGELOG.md", "CONTRIBUTING.md", "SECURITY.md"
  end

  test do
    assert_match "Praxis", shell_output("#{bin}/praxis --help 2>&1", 0)
  end
end
