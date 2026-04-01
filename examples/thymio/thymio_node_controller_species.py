# type: ignore

from typing import Any


# same [Python] controller
def control(thymio: Any, dt: float) -> None:
    ...


# for different robots (10 seconds)

# 1. Aseba (real time)
# Sim (in coppelia or playground) or Real

import time

from pyaseba.client.thymio import Thymio

# A. create
thymio = Thymio()

# B. prepare
thymio.connect(target="...")
thymio.set_controller(control, time_step=0.1)

# C. run
time.sleep(10)

# D. clean up
thymio.close(reset=True)

# 2. Enki (batch-mode)
from pyenki import Thymio2, World

# A. create
world = World()
thymio2 = Thymio2()
world.add_object(thymio2)

# B. prepare
# ... no need to connect
thymio2.control_step_callback = control

# C. run
world.run(100, 0.1)

# D. clean up
# ... no need to cleanup

# 3. Enki (real-time viz)

# C. run (qt window)
world.run_in_viewer(time_step=0.1, duration=10)

# or run (notebook)
rfb = EnkiRemoteFrameBuffer(world=world)
await rfb.run_async(time_step=0.1, duration=10)
