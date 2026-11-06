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
- Crash en 0x006E2DB4 (write), log 2026-11-04_05-07-55 (PC del amigo).
- 0x6E2D90 = CVehicle::operator delete (devuelve el objeto al pool de vehiculos 0xB74494),
  llamado desde un destructor de vehiculo (0x6B8960). El indice calculado sale negativo
  (EDX=0xFFFDB78D) -> el puntero NO pertenece al pool: vehiculo ya borrado (doble borrado)
  o puntero invalido.
- Hipotesis: el truco borra los autos de policia/bloqueos de carretera; esos autos son
  vehiculos de red controlados por el host y el mod tambien los borra (o ya los borro)
  -> doble eliminacion. Fix probable: en clientes no-host, no dejar que el truco elimine
  vehiculos de red (o hacer que el truco solo actue en el host / sincronizarlo).
- Contexto: el nivel de busqueda NO se sincroniza (TODO de CoopAndreas: "wanted level sync"),
  y los policias que persiguen al host existen en la PC del amigo como peds de red.
  Hipotesis: el truco limpia/elimina policias o tareas de persecucion que en realidad
  controla el host -> estado inconsistente en peds de red.

### 5. Crash al volver al juego tras tenerlo en segundo plano (alt-tab)
- Crash DENTRO de CoopAndreasSA.dll +0xE031C (write), build 097adce (Nov 4 09:31:04),
  log 2026-11-04_07-17-26 (PC del host).
- Es el inicio de un constructor (pone a 0 campos +4,+8,+0xC,+0x48,+0x4C de `this`) y `this`
  (ECX=0x027CD100) apunta a memoria no escribible -> objeto creado sobre memoria invalida.
- Contexto: SA deja de procesar el juego cuando la ventana pierde el foco, pero el hilo de
  red de CoopAndreas sigue recibiendo paquetes; al volver se procesa todo el atraso de golpe
  (crear/borrar peds y autos de red). Hipotesis: algun create/delete en ese atraso usa un
  slot/puntero ya invalido.
- Para identificar la funcion: hacer que el CI guarde un .map/.pdb de cada build (hoy no hay
  simbolos). Fix posible: que el juego siga procesando en segundo plano mientras hay red.

### 6. Crash creando un vehiculo (modelo 422, Bobcat) sobre memoria invalida
- Crash en 0x0054F1E4 (write), log 2026-11-04_05-38-33 (PC del amigo), justo tras reabrir el juego.
- 0x54F1E0 = constructor base de entidad (pone vtable 0x863C40) llamado por la cadena
  0x542281 -> 0x532A94 -> ... desde CAutomobile::CAutomobile (0x6B0AC1 en la pila), EDI=0x1A6 (422).
- `this` (0x020C1AC0) no es escribible -> el vehiculo se construye sobre memoria invalida.
- MISMO PATRON que #4 (borrar un vehiculo que no esta en el pool) y #5 (constructor del mod
  sobre memoria invalida): apunta a un bug de manejo de memoria/pool de VEHICULOS de red
  (crear/borrar vehiculos sincronizados). Prioridad alta: probablemente un unico bug de fondo.
- Contexto: ocurrio poco despues de que el amigo activara el truco del JETPACK. El jetpack no es
  un vehiculo, pero volar rapido carga zonas nuevas -> se crean/borran muchos autos (trafico,
  generadores, autos de red del otro jugador) en poco tiempo. Sospecha: el bug aparece con
  mucho movimiento de creacion/borrado de vehiculos (alejarse/moverse rapido).

### 7. NPC y policia que "se teletransportan" cuando un jugador se aleja
- Peds (incl. policia) aparecen/desaparecen de golpe cerca del otro jugador, sin estar tan lejos.
- El trafico que se limpia al no mirar puede reaparecer delante de un jugador.
- Relacionado con como se reparte el streaming de peds/vehiculos entre host y clientes.

### Diagnostico crashes 4/5/6/7 (con mapa de simbolos del build 097adce)
- +0xE031C de CoopAndreasSA.dll = CVehicle::CVehicle(plugin::dummy) +0x2C (constructor del SDK),
  `this` invalido. #6 = mismo pero en el ctor del juego. #4 = CVehicle::operator delete con indice negativo.
- Mecanismo: CPool::Delete(ptr) con un ptr que NO pertenece al pool escribe byteMap[idx] fuera de rango
  y deja m_nFirstFree negativo; el siguiente New() devuelve &objects[negativo] -> AV en el ctor.
  Por eso los crashes parecen aleatorios (alt-tab, lejos, jetpack): disparan la siguiente creacion.
- Causa probable: `delete m_pVehicle` sobre un puntero obsoleto (IsVTableValid no detecta slots
  reutilizados) o lectura de un CNetworkVehicle ya liberado.
- Fix: guardas en operator delete de vehiculos/peds (ignorar punteros fuera del pool y loguear el
  llamador) + validar con el pool antes de borrar en CNetworkVehicle.

## Hecho (2026-11-04, segunda sesion)
- Guardas de pool (CPoolGuard): operator delete de vehiculos/peds valida el puntero; avisos `[guard]`
  en chat/consola con el llamador. Cubre #4/#5/#6/#7.
- #2 bici: GetActiveTask() nulo en 0x523E66 parcheado.
- Trucos sincronizados (CCheatSync) + aviso "X activo un truco".
- Menu Tab: seleccion con flechas, T ir al jugador; host: Y traer, K expulsar, B banear (bans.txt).
- Propiedad por cercania (server CProximityOwnership): peds/trafico aleatorios pasan al jugador mas
  cercano; el cliente restaura IA al recibirlos. `proximity_ownership = 0` lo desactiva. Spawns
  nunca a <60 m de otro jugador. (= intento de arreglar #7; a probar)
- CI: mapas de simbolos en build/symbols, publicacion con reintentos.

### 8. Crash 0x54F493 con "autos que vuelan" + jetpack (2026-11-05)
- ECX=ESI=0 en ~CPlaceable durante el desenrollado de excepcion (funclets 0x8488xx): un constructor
  de vehiculo corrio sobre NULL. El operator new del juego devuelve NULL con el pool lleno y C++ igual
  llama al constructor. Fix: CPoolGuard::EnsureFreeVehicleSlot/EnsureFreePedSlot antes de crear
  vehiculos/peds de red (libera el mas lejano propio o no crea).
- Ventana/pantalla ancha: se usan III.VC.SA.WindowedMode.asi y GTASA.WidescreenFix.asi (ThirteenAG) en scripts/.
  CWindowMode propio queda desactivado.
- Host migration funciona: al salir el host, otro jugador pasa a ser host y siguen jugando.

### 9. Crash 0x0156173D en el limite SF/LV (moto), tambien visto al host el 04/11
- EIP dentro de un trampolin HOODLUM = CColModel::AllocateData (0x40F870) con this=NULL, EDX=0x27A6:
  el pool de CColModel (10150) se lleno (CPedModelInfo::CreateHitColModelSkinned al cargar un ped).
  Fix: pool ampliado a 25000 (push en 0x551106).

## Hecho (2026-11-05)
- Radio: solo el jugador local enciende la radio del auto (0x502302).
- Morir ya no pasa 12 h ni borra vehiculos (PassTime / ClearExcitingStuffFromArea anulados en red).
- Host sincroniza ForcedWeatherType, InterpolationValue y ms_fTimeScale.
- Votacion de trucos de mundo (CCheatVote server / CCheatSync cliente / CPrompt UI), estado de trucos
  para quien entra (CheatState), TP a pedido con aceptar/rechazar (CTeleportRequest).
- Gordo/musculoso visibles para los demas (RebuildPlayer(remote, true)).
- Copiloto trabado al salir: watchdog del cambio de arma en driveby; bInModControlledDriveby solo local.

## Pendiente / a investigar
- Copiloto: el icono de arma es normal (SA lo muestra al ir de pasajero). Bug real: a veces al subir de
  copiloto NO puede apuntar (driveby). Revisar CDriveBy::Process / StartDriveby (arma no instant-hit,
  bInModControlledDriveby) y el watchdog nuevo.
- SilentPatch: crash 0x74872F al arrancar (CPatch NOPeaba la llamada IsAlreadyRunning que SilentPatch
  engancha). Fix: solo PutRetn0(0x7468E0). EnableScriptFixes=0 en SilentPatchSA.ini.
- Avion deformado que aparece y desaparece: pedir captura/modelo; posible vehiculo de red creado antes de
  cargar el modelo o entidad con matriz corrupta.
- SensFix (punteria): pendiente de que el usuario pase el archivo.

## Pedidos de funciones
- Trucos de mundo sincronizados: policia/nivel de busqueda, cazarecompensas, autos vuelan, tiempo lento,
  etc. se aplican a todos (los personales como armas/vida/jetpack siguen siendo locales).
- Aviso "X activo un truco" visible para todos.
- Menu Tab (lista de jugadores): boton Expulsar (host).
- Submenu de teletransporte: ir a un jugador / traer a un jugador.
- Jugadores a cualquier distancia sin crashes.
- Contraseña del servidor (el túnel es público).
- Panel de admin (TP, matar, expulsar, banear) — banear por ID de jugador, no por IP
  (por el túnel todos llegan con 127.x.x.x de playit).
- Guardado por jugador en la partida del host (posición, armas, vida, dinero).
- Modo ventana / ventana sin bordes como opcion en Opciones -> Pantalla (estilo SA), guardada
  en config, + que el juego siga procesando en segundo plano mientras hay conexion (arregla el
  crash #5 de alt-tab y evita que el personaje se congele para los demas). Base: CPatch.cpp ya
  tiene InitInstance_Hook que reposiciona la ventana (modo debug --coopd0/1).
- Puente CLEO + adaptación de Misterix y Dragon Ball. Skins externas. Launcher.

## Crash #10 — 0x54F493 al robar un auto (y probablemente también #8)
- Causa real: los constructores de `CTaskSimpleCarSetPedInAsDriver/AsPassenger/CarSetPedOut` registran
  una referencia al vehículo en la dirección de la tarea. CoopAndreas las crea en la pila y plugin-sdk
  no tiene destructor para ellas → referencia colgando en la pila. Al borrar ese vehículo,
  `CEntity::ResolveReferences` escribía NULL en la pila (aquí, el ESI guardado = `this`) → crash.
- Arreglo: `CUtil::DestroyStackTask(&task)` (destructor del juego, vtable[0] con flag 0) en los 8 sitios.

## Lote 2026-11-05 (madrugada) — menú F2, desbloqueo, skins, propiedades
- **Menú F2** (`client/src/UI/CModMenu.cpp`): Trucos (pasa por CCheatSync: los de mundo piden votación),
  Vehículos (todos los modelos 400-611 por categoría, `CCheat::VehicleCheat`), Skin (`CSkin`, modelo ped
  con animaciones de CJ, sincronizada con `PLAYER_SKIN`; el server la reenvía a quien entra), Jugador
  (curar, dinero, estrellas, armas, jetpack, paracaídas), Teletransporte (lugares fijos), Guardar partida
  (`ACTIVATE_SAVE_MENU`). El guardado siempre escribe modelo 0 (CJ) para el jugador.
- **Desbloqueo** (`CUnlockAll`): al conectar: stat 181=4 (ciudades, sin estrellas), quita IPL BARRIERS1/2,
  enciende las carreteras que INTRO apaga, habilita los 50 enex que la historia va abriendo, stats
  (vida máx, resistencia, pulmón, armas, conducción, vuelo, moto, bici) a 1000. Se reaplica cada 15 s.
- **Propiedades** (`CPropertySync`): vigila `$already_bought_house[32]` (global 728). Al comprar uno,
  todos la reciben: el host la "recoge" (AddToCollectedPickupsArray + quitar pickup → BUY_PRO lanza la
  misión de compra normal y su estado se sincroniza como siempre); los clientes marcan la global,
  quitan el pickup, habilitan el enex y activan el garaje. El server guarda la lista para los que entran.
- **Radio tras morir** (`CFixes`): si no estoy en un vehículo y hay radio activa → StopRadio.
- **Encerrado en interior** (`CFixes`): un cliente dentro de un interior nunca queda con enex deshabilitados.
- Pendiente: misión de Ryder (falta saber qué falla), mira/SensFix, avión deformado, duplicado de autos.

## Lote 2026-11-05 03:50
- Skin crasheaba: ahora se cambia igual que el opcode 09C7 (DeleteRwObject y SetModelIndex virtuales,
  modelo -1 antes, se conserva m_nAnimGroup 0x4D4).
- Radio: todas las llamadas que encienden la radio (0x4F5BA0 desde 0x5022C9/0x642113/0x648A36/0x64CAAA y
  0x4F5B20 desde 0x45F159/0x50237C) pasan por un filtro: solo si el vehículo es del jugador local. Causa:
  las tareas de los jugadores remotos corren con CWorld::PlayerInFocus apuntando a ellos.
- Autos que solo ve el que entra: si llega una actualización de un vehículo remoto cuya copia local el
  juego borró, se vuelve a crear (CNetworkVehicle::EnsureGameVehicle, máx. cada 3 s).
- Misiones: LocateAllPlayersOnFoot3D ahora basta con UN jugador (radio mínimo 1.5 m); si lo pisa un
  cliente, el host es llevado al marcador. Menú F2 > Misiones (solo host) lanza cualquier misión 11-112
  y permite cancelar la actual.
- Menú: trucos por categoría, repetición de flechas 350 ms + 110 ms, teletransporte al marcador del mapa.

## 2026-11-05 04:15
- **CJ gordo al iniciar**: era mío. CUnlockAll ponía el stat 21 a 1000 creyendo que era "pulmón"; 21 es
  FAT (pulmón = 225). Ahora usa las constantes eStats y, si encuentra FAT >= 999, lo baja a 0 y reconstruye.
  Menú > Jugador > "Cuerpo normal" por si queda alguien gordo.
- **Menú se movía solo**: `consume()` ponía OldKeyState a 0, así que cada frame parecía "recién pulsada".
  Ahora el menú lleva su propia memoria de teclas. Flecha izquierda ya no vuelve atrás.
- **Skin se colgaba**: probablemente LoadAllRequestedModels bloqueante. Ahora pide el modelo y lo aplica
  cuando el streaming lo tiene (CSkin::Process, timeout 8 s con mensaje).
- Cada jugador remoto tiene su propio CPedClothesDesc (todos los CPlayerPed comparten el puntero del
  template 0xA430B0 → vestir a un remoto vestía al local).
- OJO instalación: device_commit_files a veces dice "written" y el archivo no cambia; verificar tamaño.

## 2026-11-05 06:10
- Ropa al reconectar: el server solo guarda la ropa cuando llega un RebuildPlayer; al cargar partida y
  conectar no se mandaba ninguno → ahora se envía la desc actual en el handshake.
- Skin local: ahora vía opcode SET_PLAYER_MODEL (0x109C7, el mismo que usan los selectores CLEO), solo
  a pie; printf "[skin] apply/done" para ver en consola dónde se cuelga si sigue pasando.
- Pendiente: botín de robo de casas no se ve en otros; avión blanco deformado (captura: Andromada/
  Shamal con geometría estirada cerca de Grove); mira.

## 2026-11-05 06:25
- Skin "no hace nada": SetLocal rechazaba si `m_pVehicle != null`, pero CPed::m_pVehicle se queda con el
  último auto al bajarse → siempre decía "bájate". Ahora solo mira bInVehicle.
- Crash 06:11 (build 07:14 UTC) con Cesar: EIP en CRunningScript__Process_Hook (DLL+0x56D90), script #94 con
  IP 0x01C0FFFF. Era la build con SetModelIndex a mano; la actual usa SET_PLAYER_MODEL (0x109C7).
- Botín de robos: nuevo paquete PLAYER_HOLD. Local: CPed::GetHoldingTask → modelo + offset/hueso/anim.
  Remoto: crea un CObject sin colisión y le da CTaskSimpleHoldEntity (ctor 0x6913A0, CTask new 0x61A5A0)
  en la tarea secundaria 4. Modelo 0 = suelta y borra el objeto.

## 2026-11-05 07:10 — skin colgada: causa encontrada con CoopAndreas_trace.log
- Traza: "SetLocal 292 (Cesar), loaded=0" → 2 frames → se corta dentro de CGame::Process (streaming).
- Los personajes de historia (1-6, 8, 42, 65, 74, 86, 119, 149, 208, 265-273, 289-299) NO tienen modelo propio:
  se cargan por nombre en un slot especial (023C LOAD_SPECIAL_CHARACTER → modelos 290-299). Pedir el
  modelo 292 directamente colgaba el streaming. Ahora: skin 1000+i = personaje por nombre (sweet, cesar...),
  slot 10 para mí y 9..6 para los demás; 09C7 con el modelo del slot. Solo se listan los nombres que
  existen en ms_pExtraObjectsDir y los modelos normales con CdSize != 0.

## 2026-11-05 07:45 — skins importadas
- CoopAndreas\skins\NOMBRE.dff + NOMBRE.txd. Al arrancar (hook 0x5B8E1B, antes de CStreaming::LoadCdDirectory)
  se empaquetan en CoopAndreas\coop_skins.img (IMG v2) como csk00..csk98 y se registran con
  AddImageToList. Al no tener entrada en ninguna IDE quedan en ms_pExtraObjectsDir y se cargan como
  personaje especial (023C + 09C7), igual que Cesar/Sweet. Id = 2000 + FNV1a(nombre) % 60000.
- Si un remoto usa una importada que no tengo: aviso en el chat y se lo ve como CJ.
- Pendiente probar con un dff real. Bug visual conocido: brazos estirados en el ragdoll tras matar/morir
  con skin especial (se arregla re-eligiendo la skin).

## 2026-11-05 08:00 — Kamehameha nativo
- El .cs de Alan Cortes es todo visual (coronas, checkpoints de carrera recoloreados por memoria, humo,
  luces, cámara en un heli invisible) + explosiones tipo 8 sin sonido. Correrlo en cada PC no sirve para
  que los demás lo vean, así que se reimplementó en C++ (CKamehameha) con paquete PLAYER_POWER (fase +
  heading/pitch a 10 Hz). Explosiones solo en el PC del que lo lanza; los demás ven FX "explosion_medium".
  NO instalar el Kamehameha.cs de CLEO (se duplicaría).
- Gravity gun (ggun.cs, Mr. Seemann): agarra autos/peds/objetos y los mueve con velocidad → en coop hace falta
  tomar la propiedad de sincronización del auto/ped agarrado. Pendiente.
- Instalación: el primer archivo de un device_commit_files con varios archivos a veces queda viejo;
  verificar md5 y reintentar de a uno.

## 2026-11-05 08:45 — skins faltantes
- La carpeta tiene ~225 pares; el límite era 99 → ahora 999 (csk000..). El IMG se arma copiando archivo por
  archivo (antes leía todo a memoria: cientos de MB en un proceso de 32 bits) y se reutiliza si la carpeta no
  cambió (coop_skins.idx con firma nombre+tamaño+fecha). Directorio de objetos extra 550 → 4000 entradas.
- Ignoradas por nombre: "HDTommySAb1.dff" + "HDTommySA1024V1.txd" (nombres distintos) y "SONIC1 (2).dff" (sin txd).

## 2026-11-05 17:50
- Kamehameha: Alt+H inicia, mantener H dispara. Sonido = audio de misión 6402 (slot 3, igual que el .cs);
  cámara fija detrás/arriba apuntando por el rayo (SET_FIXED_CAMERA_POSITION + POINT_CAMERA_AT_POINT, restaurada con
  RESTORE_CAMERA_JUMPCUT), SHAKE_CAM 90 por frame mientras dispara; bExplosionProof al lanzador y sin explosiones
  a < 6 m (se hería a sí mismo).
- Gravity gun nativa (CGravityGun, Alt+G o F2 > Armas): pistola = objeto 348 + 2976 + 3×2956 en la mano (hold
  sync bone 6/16, los remotos añaden las partes). RMB agarra/suelta (raycast cámara 25 m: autos, peds no-jugador,
  objetos), LMB lanza. Mantener = m_vecMoveSpeed hacia el punto a 4 m, sin gravedad. Sync de lo agarrado: por
  propiedad por proximidad (el que lo sostiene es el más cercano).
- Menú > Armas: todas las armas con munición (RequestModel + LoadAllRequestedModels + GiveWeapon).

## 2026-11-05 18:45
- Kamehameha: GunMove_BWD es "caminar hacia atrás": ahora se congela al 27 % cada frame (SET_CHAR_ANIM_SPEED 0 +
  CURRENT_TIME 0.27) y se anula la velocidad; sonido 6402 se recarga y repite mientras dura; cámara más baja,
  sobre el hombro; el mouse gira/levanta el rayo (se captura en el hook de UpdatePads antes de que lo use la cámara).
- Gravity gun: igual que el original, ocupa la escopeta de combate (arma 27): Alt+G / menú la da y activa el modo;
  funciona mientras esa arma está en la mano (SET_PLAYER_FIRE_BUTTON off). Arma en la mano con 070A
  TASK_PICK_UP_OBJECT bone 6 flags 16 "NULL" (con el ctor 0x6913A0 y anim 0 se caía al suelo). Apuntar + clic izq
  agarra, clic izq lanza, E suelta.
- Rueda de emotes (CEmoteWheel): mantener B, mover el mouse, soltar. 12 emotes (DANCING, SMOKING, BAR, PAULNMAC,
  ON_LOOKERS, RAPPING, COP_AMBIENT, BEACH, PED). Se cancela al moverse. Sync con PLAYER_POWER power=1.

## 2026-11-05 19:00
- Kamehameha: cámara lateral baja (7 m a la izquierda, 3 m atrás) como la referencia; rayo en cono (fino en las
  manos, más ancho lejos, núcleo blanco + brillo azul + borde arcoíris, sale disparado en 0.35 s); bola de carga
  más chica; sonido repetido sin recargar (como el play_wav por frame del original); pitch del mouse corregido;
  explosiones desde 3.5 m.
- Gravity gun: alcance 40 m; rayo desde la cámara y, si no acierta, el más cercano a la línea de mira (cono 7°,
  más tolerancia para autos).
- Emotes: eje vertical corregido; objetos: cigarrillo 1485 CJ_CIGGY + fx "cigarette_smoke", botella 1484
  CJ_BEAR_BOTTLE, mear fx "petrolcan" (nombres de fx sin confirmar).

## 2026-11-05 19:10
- Gravity gun como el original: coronas verdes en la boca (0.375) y en las 3 garras (0.075), (16,64,32) vacía /
  (64,255,128) sosteniendo + luz; garras se abren al agarrar; al agarrar/lanzar: 4 trazas de bala, chispas x2,
  sonidos 1058 (agarrar) / 1052 (lanzar) / 1053 (soltar). Los remotos dibujan el brillo (PLAYER_POWER power=2).
  Alcance 60 m, cono 10°. Los peds agarrados ya no reciben la anim KO (parecían muertos) y son a prueba de
  golpes/colisión mientras se sostienen; mueren al lanzarlos (original).
- Kamehameha: cámara intermedia (3.2 m al costado, 3.6 m atrás, 0.8 m arriba) y rayo ~1.7x más grueso.
- Emotes: el fx se crea con una matriz propia que sigue al ped (petrolcan emite por +Y = adelante del ped);
  cigarette_smoke existe en effects.fxp (antes no se veía por la matriz/culling).
- Sin tutorial de hospital/policía: $wasted_help (119) y $busted_help (126) = 1 y se quitan sus pickups (669-671).

## 2026-11-05 19:35
- Kamehameha nuevo: carga hasta 8 s (nivel 0..1; tamaño = 0.3+1.4·nivel, largo = 20+60·m; 4 s = como antes), disparo
  8 s, cancelable. Teclado: Alt+H carga, H lanza, H/clic corta. Arma: manopla (arma 1) con F2 > Armas >
  Kamehameha: clic der. carga (soltar cancela), clic izq. lanza/corta; fuego de la manopla bloqueado.
  Sonidos propios CoopAndreas\sounds\kame_cargar.wav / kame_lanzar.wav por bass.dll (CSoundFile; volumen SFX
  del juego, distancia y paneo para los remotos). PLAYER_POWER lleva "level" (server recompilado).
  El mouse solo se come durante el disparo (al cargar mueve la cámara normal).
- Skins: si un slot del ped ya tiene ese personaje se reutiliza; si no, UNLOAD + LOAD en el otro slot (cargar el
  mismo personaje en dos slots compartía el TXD y quedaba blanca). La skin guardada se aplica 3 s después de que el
  juego es jugable (sin cutscene/pantalla ancha/controles bloqueados); cada 1 s se valida y se re-aplica si una
  cutscene u otra cosa cambió el modelo (eso era el "CJ deformado" al empezar partida nueva).
- Menú: izquierda/derecha saltan una página; títulos sin textos explicativos.
- Emotes: bailes de la discoteca (GFUNK/RUNNINGMAN/WOP dance_loop), DANCING dnce_M_d, STRIP STR_Loop_A, RAPPING;
  fumar = SMOKING M_smk_drag (M_smkstnd_loop era el de "dolor de cabeza"). Nombres verificados en anim.img.

## 2026-11-05 19:50 — volar y super velocidad (CFlight)
- F2 > Jugador: "Volar (doble salto)" y "Super velocidad", por jugador. Alt + rueda = nivel 1..10 (barra estilo HUD).
- Volar: saltar de nuevo en el aire (< 2 s del primer salto). Dirección = cámara, W/S/A/D, salto sube, agacharse (C)
  baja, sprint = turbo. m_vecMoveSpeed cada frame sin gravedad; anims PARACHUTE PARA_float / FALL_SkyDive_Accel.
  Aterriza bajando a < 1.3 m del suelo; a prueba de golpes mientras vuela + 1.5 s.
- Velocidad: m_fSpeed de las asociaciones walk/run/sprint (ids 0-2) = 1 + 0.45·(nivel-1).
- Sync PLAYER_POWER power=3 (bit0 vuela, bit1 velocidad, level).
- Bailes GFUNK/RUNNINGMAN/WOP: dance_loop es idéntico en los tres (md5); ahora encadenan Dance_B1..B16 de su archivo.

## 2026-11-05 20:10
- Gravity gun: solo agarra entidades visibles, con modelo (m_pRwObject) y colisión; objetos con esfera de colisión
  entre 0.05 y 12 m; nunca las piezas de armas/props del coop.
- Volar: más rápido (0.15 + 0.17·nivel m/frame, turbo x1.6); hacia adelante sigue la cámara completa (también
  arriba/abajo) y el cuerpo se inclina en esa dirección (matriz del ped rotada sobre su eje derecho después del
  procesado); hacia atrás más lento (0.55) con FALL_skyDive; laterales FALL_SkyDive_L/R; flotar (salto/C) = mitad de
  la velocidad de la barra. Los demás ven la inclinación (pitch en PLAYER_POWER).

## 2026-11-05 20:15
- Niveles separados: Alt+rueda cambia el de vuelo volando y el de carrera en el suelo.
- Flotar quieto = losa invisible (objeto 3095, SET_OBJECT_VISIBLE false, congelada) bajo los pies: el juego cree
  que CJ está parado → armas, apuntar y kamehameha funcionan. Salto/agacharse se leen en el hook de pad y no
  llegan al juego mientras se vuela (no salta ni se agacha sobre la losa). Doble toque de agacharse = caer.
  Kamehameha solo flotando (no volando). Remotos: tipo 5 = parado en el aire con sus anims normales.
- Menú > Vehículos > Tunear: reparar, indestructible, nitro x2/x5/x10, hidráulica, escapes/ruedas, colores,
  pinturas especiales (se sincronizan por los paquetes de componentes/colores existentes).
- Gravity gun: distancia de sostén = radio del modelo + 3.5 m (mín. 4) para que lo grande no te golpee.

## 2026-11-05 21:30
- Flotar quieto: pose PARA_float sin gravedad ni velocidad (antes losa + gravedad → "parado en el aire" y caídas al
  soltar). La losa invisible solo mientras se apunta un arma de fuego/arrojadiza (clic der.), con CJ enderezado y 6 cm
  de margen bajo los pies (el solapamiento lo lanzaba). Hacia atrás = PARA_decel (distinta de los costados).
- Kamehameha volando: pitch mínimo -1.35 rad (antes -0.26).
- Gravity gun: el rayo ya no considera objetos; en el cono primero gente/autos con línea de visión libre; objetos
  solo si no hay nada más y están en pantalla. Los peds lanzados ya no mueren: 3 s a prueba de golpes/caídas.

## Build 6816347
- Vuelo: se quitó el suelo invisible. Al flotar quieto y apuntar con pistola/subfusil (armas que el jetpack permite) se usa la tarea de jetpack del juego (animaciones y disparo del jetpack). Al soltar, vuelve a flotar. Kamehameha sigue permitido.
- Vuelo: el ángulo de la cámara se amplifica ×1.7, así que mirar hacia arriba sube casi en vertical.
- Flotar arriba/abajo: 0.8 × velocidad de vuelo (antes 0.5).
- Gravity gun: los autos de tráfico pierden su misión de IA, velocidad crucero 0 y motor apagado mientras se sostienen.

## Build 99696c3
- Vuelo: subiendo, la velocidad vertical crece con lo que apunta el mouse hacia arriba (hasta 2.3x recto arriba).
- Kamehameha: animacion real del mod DBZ. CoopAndreas\anims\*.ifp se meten en coop_skins.img (bloques de animacion propios, max 19 letras de nombre). Poses: carga al 25%, disparo al 70%. Si el ifp falta, vuelve a GunMove_BWD.
- Iconos HUD propios (CWeaponIcons): hook en CHud::DrawWeaponIcon -> RwTexDictionaryFindNamedTexture (0x58D84B). Texturas 64x64 RLE en WeaponIconData.h.
- Genkidama (CGenkidama cliente, CGenkidama servidor, paquete GENKIDAMA): Alt+K flotando quieto. Encuesta (CPrompt Y/N, 10 s). Todos dan -> 10 s, mitad -> 20 s, menos -> denegada. Cooldown 5 min en el servidor. Bola de coronas sobre la cabeza, barra para todos. Al 100%: sonido lanzar + anim WEAPON_throw (bloque GRENADE), 2 s despues vuela a 50 m/s hasta chocar (max 12 s). Explosion: flash, SHAKE_CAM, anillo de explosiones (3 s, hasta 100 m), coronas, sonido global. <22 m muerte, <120 m lanzados al aire (FALL_skyDive, sin volar, mueren al aterrizar). El lanzador sale volando pero no muere (explosion proof 4 s). Autos <45 m explotan, el resto vuelan. Peatones <60 m mueren.
- Los wav al copiarse al PC ganan un chunk C2PA (5766 bytes): inofensivo.

## Build 56f89bb
- Vuelo: pitch de camara x2.4, subida recta = 3.5x la velocidad de la barra; flotar arriba/abajo 0.95x; nivel < 3 hacia adelante usa FALL_SkyDive (pose tranquila, como los costados).
- Kamehameha: poses invertidas (carga 70%, disparo 25%).
- Genkidama: anims de yuanqidan.ifp (dbz_1 carga, dbz_2 lanzar). Explosion nivel bomba: onda que se expande hasta 260 m en 4 s y golpea al pasar; <60 m muerte; peatones <200 m mueren; autos <170 m destruidos (<100 m explotan), todos salen volando; hongo de explosiones FX (explosion_large, prt_smoke_huge) hasta 150 m de alto, 45 fuegos que se apagan a los 30 s. Lanzador inmune a explosion/fuego 10 s. Cooldown 3 min.
