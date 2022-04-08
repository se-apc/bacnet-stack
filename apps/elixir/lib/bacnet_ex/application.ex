defmodule BacnetEx.Application do
  @moduledoc """
  Documentation for `BacnetEx`.
  """
  use Application
  require Logger

  def start(_type, _args) do
    children = [
        BacnetEx
    ]
   
    Supervisor.start_link(children, strategy: :one_for_one)
  end
end
