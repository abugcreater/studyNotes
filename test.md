@startuml

[*]-> State1
State1 --> [*]
State1 : dog
State1 : cat

State1 -> State2
State2 --> [*]

@enduml
