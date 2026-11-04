# Notas de pruebas (versión privada)

Registro de lo probado con amigos: qué funciona, qué falla y pistas técnicas.
Direcciones = gta_sa.exe 1.0 US (HOODLUM, MD5 170b3a9108687b26da2d8901c6948a18).

## Funciona (prueba 2026-11-04, 2 jugadores por túnel playit.gg)
- Conexión por túnel playit (host + 1 amigo).
- Se ven las armas que el otro saca con trucos.
- Se ve la barra de vida actualizada (HESOYAM).
- Se ve el jetpack del otro.
- El amigo puede subirse a autos spawneados por el host.

## Errores / pendientes

### 1. Crash con el truco "recompensa por tu cabeza" (BAGOWPG)
- Crash en 0x0063A04B (write, ECX=0) dentro de 0x63A040, llamado desde 0x6715B0.
- 0x6715A6: `operator new(0x64)` (0x41B7C0) devolvió NULL -> el juego no lo comprueba.
- Hipótesis: pool de eventos/IA agotado (todos los peatones atacan a la vez; con jugadores
  de red hay más objetivos). Fix probable: ampliar límites de pools (estilo limit adjuster)
  y/o bloquear/sincronizar trucos de caos de peatones.
- Además: los trucos NO se sincronizan (TODO de CoopAndreas: "cheat code sync").

### 2. Crash al subir dos jugadores al mismo vehículo (bici)
- Crash en 0x00523E6B (read, EAX=0), log 2026-11-04_07-02-12 (PC del host).
- 0x523E5D: `ped->m_pIntelligence (+0x47C) -> taskManager (+4) -> GetActiveTask()` (0x681720)
  devuelve NULL y se desreferencia sin comprobar (compara el id de tarea con 0x2BD).
- Hipótesis: al ir de pasajero en una bici (sincronización de pasajero), el ped queda un
  frame sin tarea activa y este código (zona de cámara/ped, 0x523xxx) no lo tolera.
  Fix probable: hook en ese punto que salte el bloque si GetActiveTask() == NULL, y revisar
  CPassengerEnter para bicis/motos.

### 3. Autos de Ryder y Sweet duplicados frente a la casa de CJ
- Los generadores de autos / script los crean en cada PC -> aparecen dos copias.
- Revisar sincronización de car generators (CPatch desactiva CTheCarGenerators::Process para
  no-host; ver si el script MAIN/INITIAL crea esos autos en el cliente).

### 4. Crash con el truco "quitar nivel de busqueda" (TURNDOWNTHEHEAT)
- Lo uso el amigo (cliente) sin tener policias; quien tenia nivel de busqueda era el host.
- Falta la direccion del crash (pedir el log de CoopAndreas_crashes de su PC).
- Contexto: el nivel de busqueda NO se sincroniza (TODO de CoopAndreas: "wanted level sync"),
  y los policias que persiguen al host existen en la PC del amigo como peds de red.
  Hipotesis: el truco limpia/elimina policias o tareas de persecucion que en realidad
  controla el host -> estado inconsistente en peds de red.

## Pedidos de funciones
- Contraseña del servidor (el túnel es público).
- Panel de admin (TP, matar, expulsar, banear) — banear por ID de jugador, no por IP
  (por el túnel todos llegan con 127.x.x.x de playit).
- Guardado por jugador en la partida del host (posición, armas, vida, dinero).
- Puente CLEO + adaptación de Misterix y Dragon Ball. Skins externas. Launcher.
