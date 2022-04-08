defmodule BacnetEx do
  @moduledoc """
  Documentation for `BacnetEx`.
  """
  use GenServer
  require Logger

  # a random hard-coded object Instance, e.g. 23:
  @device_object_instance 23

  # List of possible functions, which can be called from the Elixir driver Module to the C Driver.
  # Each number is similarly declared in port_driver.c as an Enum.
  @set_dev_object 1
  @get_dev_object 2
  @device_init 3
  @set_who_is_handler 4
  @set_unrec_service 5
  @set_read_handler 6
  @set_iam_handler 7
  @set_final_handlers 8
  @dl_init 9
  @send_iam 10
  @setup_bacnet_device 11
  @send_who_is 12
  @check_rx_data 13

  # list of commands that don't have any arguments, we pass in a default arg of `0`
  @void_commands [@get_dev_object, @device_init, @set_who_is_handler, @set_unrec_service, @set_read_handler, 
    @set_iam_handler, @set_final_handlers, @dl_init]

  @signed_function 1
  @unsigned_function 2

  # list of functions which are supported in the synchronous Driver call:
  @func_list [@signed_function, @unsigned_function]

  def start_link(args) do
    ## name same as defined in the driver_name field of ErlDrvEntry struct in driver library:
    driver_lib = "libbacnet-stack"

    res =
      case :erl_ddll.load_driver("../../_build/", driver_lib) do
        :ok ->
          IO.puts "Driver Loaded"
          :ok
        {:error, :already_loaded} ->
          IO.puts "Driver already loaded"
          :ok
        error ->
          IO.puts("could not load driver: #{inspect error}")
          :error
      end

    if res == :ok do
      GenServer.start_link(__MODULE__, driver_lib, name: __MODULE__)
    end
  end


  def init(driver_lib) do
	  # use the BIF open_port() to spawn process for the application.
	  # "port" variable contains descriptor to the port being used for the Inter-process comms
	  #port = :erlang.open_port({:spawn, driver_lib}, [])
	  port = Port.open({:spawn, driver_lib}, [{:packet, 2}])

    IO.puts "PORT #{inspect port} opened for #{driver_lib}"
    state = %{lib: driver_lib, port: port}

    {:ok, state}
  end

  # This performs the same steps as iam application in bacnet stack:
  def setup_bacnet_device(instance \\ @device_object_instance)
  def setup_bacnet_device(instance) do
    set_device_object_instance_number(instance)

    # This is just to verify the object we set is the same as returned:
    ret_instance = get_device_object_instance()
    IO.puts "Object Instance: #{inspect ret_instance}"

    device_init()
    set_who_is_handler()
    set_unrec_service_handler()
    set_read_prop_handler()
    set_iam_handler()
    set_final_handlers()
    dl_init()
    send_iam(instance)
    :ok
  end

  # same as setup_bacnet_device(), but uses the implementation in C.
  def setup_bacnet_device_native(instance \\ @device_object_instance)
  def setup_bacnet_device_native(instance) do
    GenServer.call(__MODULE__, {@setup_bacnet_device, instance})
  end

  # follow up this call with check_rx_data() to view any Devices that may have responded.
  # may need to be called a few times to read a response.
  def who_is() do
    GenServer.call(__MODULE__, {@send_who_is, @device_object_instance})
  end
  
  def check_rx_data() do
    # timeout of 1000ms can be changed to longer once it's perfomed in a separate Task.
    # the decoding in the C function must match the data-type of the variable passed in.
    GenServer.call(__MODULE__, {@check_rx_data, 1000})
  end

  def set_device_object_instance_number(instance) do
    GenServer.call(__MODULE__, {@set_dev_object, instance})
  end

  def get_device_object_instance() do
    GenServer.call(__MODULE__, @get_dev_object)
  end

  def device_init() do
    GenServer.call(__MODULE__, @device_init)
  end

  def set_who_is_handler() do
    GenServer.call(__MODULE__, @set_who_is_handler)
  end

  def set_unrec_service_handler() do
    GenServer.call(__MODULE__, @set_unrec_service)
  end

  def set_read_prop_handler() do
    GenServer.call(__MODULE__, @set_read_handler)
  end

  def set_iam_handler() do
    GenServer.call(__MODULE__, @set_iam_handler)
  end

  def set_final_handlers() do
    GenServer.call(__MODULE__, @set_final_handlers)
  end

  def dl_init() do
    GenServer.call(__MODULE__, @dl_init)
  end

  # must be called after all the other setup functions, as per setup_bacnet_device()
  def send_iam(device_id \\ @device_object_instance)
  def send_iam(device_id) when is_integer(device_id) and device_id < 4194303 and device_id >= 0 do
    GenServer.call(__MODULE__, {@send_iam, device_id})
  end

  # A test function for checking data types generated in C depending on the value passed as `var`
  def sync_call_func(func, var) when func in @func_list do
    GenServer.call(__MODULE__, {:sync_call_func, func, var})
  end

  def test_func(value) do
    GenServer.call(__MODULE__, {:call_test_func, value})
  end

  def test_unassigned_func(value) do
    GenServer.call(__MODULE__, {:call_unassigned_func, value})
  end

  def handle_call({@setup_bacnet_device, instance}, _from, state = %{port: port}) do
    Port.command(port, [@setup_bacnet_device, instance])
    {:reply, :ok, state}
  end

  def handle_call({@send_who_is, instance}, _from, state = %{port: port}) do
    Port.command(port, [@send_who_is, instance])
    {:reply, :ok, state}
  end

  def handle_call({@check_rx_data, instance}, _from, state = %{port: port}) do
    Port.command(port, [@check_rx_data, :erlang.term_to_binary(instance)])
    {:reply, :ok, state}
  end

  def handle_call({@set_dev_object, instance}, _from, state = %{port: port}) do
    Port.command(port, [@set_dev_object, instance])
    {:reply, :ok, state}
  end

  def handle_call({@send_iam, device_id}, _from, state = %{port: port}) do
  ##   enforce sizing here
    Port.command(port, [@send_iam, device_id])
    {:reply, :ok, state}
  end

  # command is an integer (e.g. from the C Enum of function maps)
  def handle_call(command, _from, state = %{port: port}) when command in @void_commands do
    Port.command(port, [command, 0])
    {:reply, :ok, state}
  end

  def handle_call({:call_test_func, value}, _from, state = %{port: port}) do
    Port.command(port, [666, value])
    {:reply, :ok, state}
  end

  def handle_call({:call_unassigned_func, value}, _from, state = %{port: port}) do
    Port.command(port, [value, value])
    {:reply, :ok, state}
  end

  # only supporting @signed_function and @unsigned_function commands in the Synchronous function calls
  def handle_call({:sync_call_func, func, value}, _from, state = %{port: port}) when func in @func_list do
    ret = :erlang.port_call(port, func, value)
    IO.puts "Sync call returned #{inspect ret}"
    {:reply, :ok, state}
  end

  def handle_info({_port, {:data, [integer]}}, state) when is_integer(integer) do
    IO.puts "received integer #{inspect integer}"

    {:noreply, state}
  end

  # depending on the size of the return_value list, the value can be interpreted back into Elixir
  def handle_info({_port, _data = {:data, return_value = [integer| _tail]}}, state) when is_integer(integer) do
    # e.g. [21, 0, 0, 0] is a uint32_t value of 21.
    IO.puts "received integer #{inspect integer} in #{inspect return_value}"

    {:noreply, state}
  end

  def handle_info({_port, data = {:data, []}}, state) do
    IO.puts "received [] data"
    {:noreply, state}
  end

  def handle_info({_port, data = {:data, return_value}}, state) when is_list(return_value) do
    IO.puts "received data #{inspect data}"
    {:noreply, state}
  end
end
