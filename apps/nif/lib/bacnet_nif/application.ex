defmodule BacnetNif.Application do
  @moduledoc """
  Documentation for `BacnetNif`.
  """
  use Application
  require Logger

  def start(_type, _args) do
    children = [
        BacnetNif
    ]
    Supervisor.start_link(children, strategy: :one_for_one)
  end
end
