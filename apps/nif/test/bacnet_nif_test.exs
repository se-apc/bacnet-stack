defmodule BacnetNifTest do
  use ExUnit.Case
  doctest BacnetNif

  test "greets the world" do
    assert BacnetNif.hello() == :world
  end
end
