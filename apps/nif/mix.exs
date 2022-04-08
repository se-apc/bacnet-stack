defmodule BacnetNif.MixProject do
  use Mix.Project

  def project do
    [
      app: :bacnet_nif,
      description: "Elixir BACnet Nif Demo",
      version: "0.1.0",
      elixir: "~> 1.12",
      make_targets: ["build_lib"],
      compilers: [:elixir_make] ++ Mix.compilers(),
      start_permanent: Mix.env() == :prod,
      deps: deps()
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      mod: {BacnetNif.Application, []},
      extra_applications: [:logger]
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    [
       {:elixir_make, "~> 0.4", runtime: false}
    ]
  end
end
