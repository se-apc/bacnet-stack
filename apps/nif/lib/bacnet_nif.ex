defmodule BacnetNif do
  @moduledoc """
  Documentation for `BacnetNif`.

  NIFs are most suitable for synchronous functions, such as foo and bar in the example,
  that do some relatively short calculations without side effects and return the result.
  """
  use GenServer
  require Logger

  # a random hard-coded object Instance, e.g. 23:
  @device_object_instance 23

  def start_link(_args) do
    ## name same as the .so shared library
    driver_lib = "libbacnet-stack"

    res =
      case :erlang.load_nif("../../_build/libbacnet-stack", 0) do
        :ok ->
          IO.puts "Nif Loaded"
          :ok
        {:error, {:reload, text}} ->
          IO.puts "Nif already loaded: #{inspect text}"
          :ok
        {:error, {reason, text}} ->
          IO.puts("could not load Nif, error #{inspect reason}: #{inspect text}")
          :error
      end

    if res == :ok do
      GenServer.start_link(__MODULE__, driver_lib, name: __MODULE__)
    end
  end

  def init(driver_lib) do
    IO.puts "Init of NIF module for #{driver_lib}"
    state = %{lib: driver_lib}
    {:ok, state}
  end

  def setup_bacnet_device() do
    IO.puts "setup_bacnet_device() using native calls"
    set_device_object_instance_number(@device_object_instance)
    device_init()
    set_who_is_handler()
    set_unrec_service_handler()
    set_read_prop_handler()
    set_iam_handler()
    set_final_handlers()
    dl_init()
    send_iam(@device_object_instance)
    :ok
  end

  ## NIF DRIVER FUNCTIONS

  def foo(_integer) do
    IO.puts "Error: foo NIF module not loaded"
  end

  def bar(_integer) do
    IO.puts "Error: bar NIF module not loaded"
  end

  def get_var() do
    IO.puts "Error: get_var NIF module not loaded"
  end

  def set_device_object_instance_number(_instance) do
    IO.puts "Error: NIF module not loaded"
  end

  def get_device_object_instance() do
    IO.puts "Error: NIF module not loaded"
  end

  def device_init() do
    IO.puts "Error: NIF module not loaded"
  end

  def set_who_is_handler() do
    IO.puts "Error: NIF module not loaded"
  end

  def set_unrec_service_handler() do
    IO.puts "Error: NIF module not loaded"
  end

  def set_read_prop_handler() do
    IO.puts "Error: NIF module not loaded"
  end

  def set_iam_handler() do
    IO.puts "Error: NIF module not loaded"
  end

  def set_final_handlers() do
    IO.puts "Error: NIF module not loaded"
  end

  def dl_init() do
    IO.puts "Error: NIF module not loaded"
  end

  # must be called after all the other setup functions, as per setup_bacnet_device()
  def send_iam(device_id \\ @device_object_instance)
  def send_iam(device_id) when is_integer(device_id) and device_id < 4194303 and device_id >= 0 do
    IO.puts "Error: NIF module not loaded"
  end

  def handle_info(msg, state) do
    IO.puts "received data #{inspect msg}"
    {:noreply, state}
  end
end
