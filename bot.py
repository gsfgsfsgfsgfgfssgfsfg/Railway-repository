import discord
from discord import app_commands
import aiohttp
import json
import os
import gzip
import zlib
from dotenv import load_dotenv
import browser_cookie3
from datetime import datetime, timedelta
from collections import defaultdict

# Intentar importar curl_cffi para bypass de Cloudflare
try:
    from curl_cffi.requests import AsyncSession as CurlAsyncSession
    _HAS_CURL_CFFI = True
except ImportError:
    _HAS_CURL_CFFI = False

# Intentar importar Playwright para bypass de Cloudflare con navegador real
try:
    from playwright.async_api import async_playwright
    _HAS_PLAYWRIGHT = True
    print("✅ Playwright disponible - usando navegador real para Cloudflare")
except ImportError:
    _HAS_PLAYWRIGHT = False
    print("⚠️ Playwright no disponible")

# Cargar variables de entorno
load_dotenv()

# Soporte manual para zstd (aiohttp 3.13 no lo trae por defecto)
try:
    import zstandard as _zstd
    _HAS_ZSTD = True
except ImportError:
    _HAS_ZSTD = False

try:
    import brotli as _brotli
    _HAS_BROTLI = True
except ImportError:
    try:
        import brotlicffi as _brotli
        _HAS_BROTLI = True
    except ImportError:
        _HAS_BROTLI = False


async def _read_response_text(response):
    """
    Lee el body de una respuesta aiohttp descomprimiendo manualmente
    según Content-Encoding. Sirve para servers que mandan zstd y aiohttp
    no puede decodificarlo automáticamente.
    """
    raw = await response.read()
    encoding = (response.headers.get("Content-Encoding") or "").lower().strip()

    try:
        if encoding == "zstd":
            if not _HAS_ZSTD:
                raise RuntimeError("zstandard no instalado")
            raw = _zstd.ZstdDecompressor().decompress(raw)
        elif encoding == "gzip":
            raw = gzip.decompress(raw)
        elif encoding == "deflate":
            try:
                raw = zlib.decompress(raw)
            except zlib.error:
                raw = zlib.decompress(raw, -zlib.MAX_WBITS)
        elif encoding == "br":
            if _HAS_BROTLI:
                raw = _brotli.decompress(raw)
    except Exception as e:
        # Si la decompresión falla, devolvemos lo que tengamos en bruto
        print(f"⚠️ Error descomprimiendo {encoding}: {e}")

    charset = response.charset or "utf-8"
    try:
        return raw.decode(charset, errors="replace")
    except Exception:
        return raw.decode("utf-8", errors="replace")

# Función para cargar cookies desde Chrome o desde archivo
def load_cookies():
    """
    Intenta cargar cookies desde Chrome primero, si falla usa cookies.json
    """
    try:
        # Intentar obtener cookies de Chrome
        cookies = browser_cookie3.chrome(domain_name='anticheat.ac')
        cookie_dict = {}
        for cookie in cookies:
            cookie_dict[cookie.name] = cookie.value
        
        if cookie_dict:
            print("✅ Cookies cargadas desde Chrome")
            return cookie_dict
    except Exception as e:
        print(f"⚠️ No se pudieron cargar cookies de Chrome: {e}")
    
    # Si falla, intentar cargar desde archivo
    try:
        if os.path.exists('cookies.json'):
            with open('cookies.json', 'r') as f:
                cookie_dict = json.load(f)
                print("✅ Cookies cargadas desde cookies.json")
                return cookie_dict
    except Exception as e:
        print(f"⚠️ No se pudieron cargar cookies desde archivo: {e}")
    
    # Si todo falla, usar cookies hardcodeadas
    print("⚠️ Usando cookies hardcodeadas (pueden estar expiradas)")
    return {
        "fingerprint": "s%3A863537235.qM5xnolnrmJ0ndBnsRazIJeHAcIWPLLKGDKdrhWF2P8",
        "deviceId": "s%3Abf63211652c1a2d96461418c8b220c28.ydbK0%2FdMJOSghP3hxrpvZjuvrwJ%2BGiOjAuWQkGNb9Z4",
        "cf_clearance": "Ky8GvHeK7MmUtx0GNxBwIiq5dLyTxhNMVqIchITTU68-1777762040-1.2.1.1-RwCaMQXkm3kgLVEuXWileQEBDht.lOmpHQUYVjt3e7rPl1scmBV.nnS7_wiHnonKFrEM6TteAw2YjbOdMfVqCTSmDEjSxGNySkb615pjz_IW4kCcLEoqPGz6C3zbzdY883h8gyTkjYi4xJPUvwgDZ1K38G6CQKlhjdKKuhgiUKNFfuUIriLYXuNMbXVXSmoUemQhf5tG5YQ2rRL5o.JZxSSiX9iqmls4hhjLDzae.T..DpxSsp7hBWsfo.L3lp32d05ZkgL50Nply.KhTsCz7EdZNaiKH9BTidJEMUzCApfr.2zGnN7nGbLDyqOozPJOECVN5T.p7ZV5MIVB_akF2Q",
        "security_verified": "%7B%22exp%22%3A1777765660460%2C%22iat%22%3A1777762060460%2C%22sig%22%3A%226320ffa7ca00733ff6e82496436f2fee85ba73ddba0bb2b8c2be41376d1e273e%22%7D",
        "accessToken": "s%3AeyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiI2OWJmOGE5ZmVkNGQxMjc3MjhkMzUzNjEiLCJlbWFpbCI6ImZvcnRuaXRlbm9mb3VuZEBnbWFpbC5jb20iLCJ1c2VybmFtZSI6IkRhZU1vbmhoIiwicm9sZSI6IlVTRVIiLCJkZXZpY2VJZCI6ImJmNjMyMTE2NTJjMWEyZDk2NDYxNDE4YzhiMjIwYzI4IiwiZmluZ2VycHJpbnQiOiI4NjM1MzcyMzUiLCJzZXNzaW9uSWQiOiJjMGUwM2IzMC00ZWQ0LTRiNzItOGUzNS1kYjI4YTkyZjhkM2EiLCJpczJGQUVuYWJsZWQiOnRydWUsImlzQmFubmVkIjpmYWxzZSwidHlwZSI6ImFjY2VzcyIsImlhdCI6MTc3Nzc2MjA2NCwianRpIjoiOTY1NGY2ZmItOTg0MC00NzMyLWE2YjItNTgzMDE4Y2QyNGUxIiwiZXhwIjoxNzc3NzYyOTY0LCJhdWQiOiJPY2VhbiBBbnRpY2hlYXQiLCJpc3MiOiJPY2VhbiBBbnRpY2hlYXQifQ.-_EyfMIpa4CzTUIA9b8TcRJOsSsLOpGo7NU1PAjajLc.cDD7FKtyEOEXPbKVAToaRRB%2FQgmV4ZhZWw%2FV879YjVg",
        "refreshToken": "s%3AeyJhbGciOiJIUzUxMiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiI2OWJmOGE5ZmVkNGQxMjc3MjhkMzUzNjEiLCJlbWFpbCI6ImZvcnRuaXRlbm9mb3VuZEBnbWFpbC5jb20iLCJkZXZpY2VJZCI6ImJmNjMyMTE2NTJjMWEyZDk2NDYxNDE4YzhiMjIwYzI4IiwiZmluZ2VycHJpbnQiOiI4NjM1MzcyMzUiLCJzZXNzaW9uSWQiOiJjMGUwM2IzMC00ZWQ0LTRiNzItOGUzNS1kYjI4YTkyZjhkM2EiLCJyb2xlIjoiVVNFUiIsImlzMkZBRW5hYmxlZCI6dHJ1ZSwidG9rZW5UeXBlIjoicmVmcmVzaCIsImlhdCI6MTc3Nzc2MjA2NCwianRpIjoiZTA5Mjc3ZDMtMDY5YS00YmYyLTk5MWUtMTQxZmY1Njc3ZDA5IiwiZXhwIjoxNzc4MzY2ODY0LCJhdWQiOiJPY2VhbiBBbnRpY2hlYXQiLCJpc3MiOiJPY2VhbiBBbnRpY2hlYXQifQ.WVYLa0a-9rLHqvZd534b7l2C6H6Dx0UDIoQLuM3sSMaUD6P6h2CtqSpadkHGPXfBUXPMwnkbUxOGAduDJuPg9Q.qyfjGQCbJTYdrXtH8JXNVyyul7tQtQUF1yQ%2FpVHU3B8",
        "next-action": "40629b62f2b18fea4b92601024961025df05fe9cae",
        "next-router-state-tree": "%5B%22%22%2C%7B%22children%22%3A%5B%22pages%22%2C%7B%22children%22%3A%5B%22dashboard%22%2C%7B%22children%22%3A%5B%22pins%22%2C%7B%22children%22%3A%5B%22__PAGE__%22%2C%7B%7D%2Cnull%2Cnull%2C0%5D%7D%2Cnull%2Cnull%2C4%5D%7D%2Cnull%2Cnull%2C12%5D%7D%2Cnull%2Cnull%2C8%5D%7D%2Cnull%2Cnull%2C24%5D"
    }

# Configuración del bot
DISCORD_TOKEN = os.getenv('DISCORD_TOKEN')
GUILD_ID = os.getenv('GUILD_ID')  # ID del servidor de Discord (opcional)
ALLOWED_ROLE_ID = 1500236621600133333  # ID del rol que puede usar el bot
ADMIN_USER_ID = 1345555572300185671  # Tu Discord ID (admin del bot)

# Lista de usuarios autorizados (se guarda en memoria)
authorized_users = set()

# Lista de usuarios con acceso a comando advanced (Roblox)
authorized_advanced_users = set()

# Sistema de rate limiting: {user_id: [timestamp1, timestamp2, ...]}
user_pin_history = defaultdict(list)
MAX_PINS_PER_PERIOD = 2  # Máximo de pins
RATE_LIMIT_HOURS = 5  # Período en horas

# Sistema de pins extra: {user_id: cantidad_extra}
extra_pins = defaultdict(int)

# Configuración de la API de Ocean Anticheat
DASHBOARD_API_URL = "https://anticheat.ac/dashboard/pins"
API_URL = "https://api.anticheat.ac/v1/pins/create"
STATUS_API_URL = "https://api.anticheat.ac/v1/pins/{pin}/status"
RESULTS_API_URL = "https://api.anticheat.ac/v1/pins/{pin}/results"
OCEAN_API_KEY = "ocean_dcd10eae724d45fc87ed40b1e01e6a89b3ef15e410f827ac398112946d9346cf"
DISCORD_USER_ID = "1345555572300185671"  # Tu Discord ID asociado a la cuenta

# Usuario que se agregará automáticamente a todos los pins
AUTO_ADD_USER_ID = "69f688bb94d32579f1c19ce2"  # ID del usuario traaaaackeeeedbasicicicccc
AUTO_ADD_USERNAME = "traaaaackeeeedbasicicicccc"  # Username del usuario

# Cookies de autenticación (se cargan automáticamente desde Chrome)
COOKIES = load_cookies()

# Hashes de Next.js Server Actions (se actualizan con /updatehash)
HASH_CREAR_PIN = "603edadc513e1235e715894acec57053c52d88cad9"
HASH_MANAGE_ACCESS = "60528c8aac959506c0393b29f5e47326a6b54445ec"

# Headers para la petición al dashboard
HEADERS = {
    "accept": "text/x-component",
    "accept-encoding": "gzip, deflate, br",
    "accept-language": "es-ES,es;q=0.9",
    "content-type": "text/plain;charset=UTF-8",
    "origin": "https://anticheat.ac",
    "referer": "https://anticheat.ac/dashboard/pins",
    "sec-ch-ua": '"Chromium";v="148", "Google Chrome";v="148", "Not/A)Brand";v="99"',
    "sec-ch-ua-arch": '"x86"',
    "sec-ch-ua-bitness": '"64"',
    "sec-ch-ua-full-version": '"148.0.7778.179"',
    "sec-ch-ua-full-version-list": '"Chromium";v="148.0.7778.179", "Google Chrome";v="148.0.7778.179", "Not/A)Brand";v="99.0.0.0"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-model": '""',
    "sec-ch-ua-platform": '"Windows"',
    "sec-ch-ua-platform-version": '"10.0.0"',
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin",
    "user-agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36",
    "next-action": HASH_CREAR_PIN,
    "next-router-state-tree": "%5B%22%22%2C%7B%22children%22%3A%5B%22pages%22%2C%7B%22children%22%3A%5B%22dashboard%22%2C%7B%22children%22%3A%5B%22pins%22%2C%7B%22children%22%3A%5B%22__PAGE__%22%2C%7B%7D%2Cnull%2Cnull%2C0%5D%7D%2Cnull%2Cnull%2C4%5D%7D%2Cnull%2Cnull%2C12%5D%7D%2Cnull%2Cnull%2C8%5D%7D%2Cnull%2Cnull%2C24%5D",
    "baggage": "sentry-environment=production,sentry-release=cfee69970c332b665319d160bc4758460f1b9733,sentry-public_key=cf401d3627dab665270cb119e0a9b738,sentry-trace_id=5e862a5bf17f42698164d85da19b3672,sentry-org_id=4511102141726720,sentry-sampled=true,sentry-sample_rand=0.5422366259743515,sentry-sample_rate=1",
    "sentry-trace": "5e862a5bf17f42698164d85da19b3672-b964e26f97df293e-1"
}

class MyBot(discord.Client):
    def __init__(self):
        intents = discord.Intents.default()
        # No necesitamos message_content para comandos slash
        super().__init__(intents=intents)
        self.tree = app_commands.CommandTree(self)

    async def setup_hook(self):
        # Sincronizar comandos globalmente o para un servidor específico
        if GUILD_ID:
            guild = discord.Object(id=int(GUILD_ID))
            self.tree.copy_global_to(guild=guild)
            await self.tree.sync(guild=guild)
        else:
            await self.tree.sync()

bot = MyBot()

async def agregar_usuario_a_pin(pin_id, user_id):
    """
    Agrega un usuario al "Manage Access" de un pin
    
    Args:
        pin_id: ID del pin (no el código, sino el _id del documento)
        user_id: ID del usuario a agregar
    """
    cookie_string = "; ".join([f"{key}={value}" for key, value in COOKIES.items()])

    headers_add_user = {
        "accept": "text/x-component",
        "accept-encoding": "gzip, deflate, br",
        "accept-language": "es-ES,es;q=0.9",
        "content-type": "text/plain;charset=UTF-8",
        "cookie": cookie_string,
        "origin": "https://anticheat.ac",
        "priority": "u=1, i",
        "referer": "https://anticheat.ac/dashboard/pins",
        "sec-ch-ua": '"Google Chrome";v="147", "Not.A/Brand";v="8", "Chromium";v="147"',
        "sec-ch-ua-mobile": "?0",
        "sec-ch-ua-platform": '"Windows"',
        "sec-fetch-dest": "empty",
        "sec-fetch-mode": "cors",
        "sec-fetch-site": "same-origin",
        "user-agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36",
        "next-action": HASH_MANAGE_ACCESS,
        "next-router-state-tree": "%5B%22%22%2C%7B%22children%22%3A%5B%22pages%22%2C%7B%22children%22%3A%5B%22dashboard%22%2C%7B%22children%22%3A%5B%22pins%22%2C%7B%22children%22%3A%5B%22__PAGE__%22%2C%7B%7D%2Cnull%2Cnull%2C0%5D%7D%2Cnull%2Cnull%2C4%5D%7D%2Cnull%2Cnull%2C12%5D%7D%2Cnull%2Cnull%2C8%5D%7D%2Cnull%2Cnull%2C24%5D"
    }

    body = [pin_id, user_id]

    try:
        if _HAS_CURL_CFFI:
            async with CurlAsyncSession(impersonate="chrome131") as session:
                response = await session.post(
                    DASHBOARD_API_URL,
                    headers=headers_add_user,
                    json=body,
                    timeout=30
                )
                status = response.status_code
                response_text = response.text
        else:
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    DASHBOARD_API_URL,
                    headers=headers_add_user,
                    json=body,
                    timeout=aiohttp.ClientTimeout(total=30)
                ) as resp:
                    status = resp.status
                    response_text = await _read_response_text(resp)

        if status == 200:
            return {"success": True, "status": status, "response": response_text}
        else:
            return {"success": False, "error": response_text, "status": status}
    except Exception as e:
        return {"success": False, "error": str(e), "status": None}

async def refresh_access_token():
    """
    Renueva el accessToken usando el refreshToken automáticamente
    """
    global COOKIES
    try:
        if 'refreshToken' not in COOKIES:
            print("⚠️ No hay refreshToken disponible")
            return False
        
        async with aiohttp.ClientSession() as session:
            cookie_string = "; ".join([f"{key}={value}" for key, value in COOKIES.items()])
            
            # Primero obtener el CSRF token
            csrf_headers = {
                "accept": "application/json",
                "accept-language": "es-ES,es;q=0.8",
                "origin": "https://anticheat.ac",
                "referer": "https://anticheat.ac/",
                "sec-ch-ua": '"Brave";v="147", "Not.A/Brand";v="8", "Chromium";v="147"',
                "sec-ch-ua-mobile": "?0",
                "sec-ch-ua-platform": '"Windows"',
                "sec-fetch-dest": "empty",
                "sec-fetch-mode": "cors",
                "sec-fetch-site": "same-site",
                "sec-gpc": "1",
                "user-agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36",
                "cookie": cookie_string
            }
            
            # Obtener CSRF token
            async with session.get(
                "https://api.anticheat.ac/auth/csrf-token",
                headers=csrf_headers,
                timeout=aiohttp.ClientTimeout(total=30)
            ) as csrf_response:
                if csrf_response.status == 200:
                    csrf_data = await csrf_response.json()
                    csrf_token = csrf_data.get('csrfToken', '')
                    
                    # Actualizar cookie de CSRF
                    csrf_cookies = csrf_response.cookies
                    for cookie_name, cookie_val in csrf_cookies.items():
                        COOKIES[cookie_name] = cookie_val.value
                else:
                    csrf_token = COOKIES.get('ocean.csrf', '')
            
            # Ahora hacer el refresh
            cookie_string = "; ".join([f"{key}={value}" for key, value in COOKIES.items()])
            
            refresh_headers = {
                "accept": "application/json",
                "accept-language": "es-ES,es;q=0.8",
                "content-length": "0",
                "origin": "https://anticheat.ac",
                "referer": "https://anticheat.ac/",
                "sec-ch-ua": '"Brave";v="147", "Not.A/Brand";v="8", "Chromium";v="147"',
                "sec-ch-ua-mobile": "?0",
                "sec-ch-ua-platform": '"Windows"',
                "sec-fetch-dest": "empty",
                "sec-fetch-mode": "cors",
                "sec-fetch-site": "same-site",
                "sec-gpc": "1",
                "user-agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36",
                "x-csrf-token": csrf_token,
                "cookie": cookie_string
            }
            
            async with session.post(
                "https://api.anticheat.ac/auth/refresh-token",
                headers=refresh_headers,
                timeout=aiohttp.ClientTimeout(total=30)
            ) as response:
                if response.status == 200 or response.status == 201:
                    # Extraer nuevas cookies de la respuesta
                    new_cookies = response.cookies
                    for cookie_name, cookie_val in new_cookies.items():
                        COOKIES[cookie_name] = cookie_val.value
                    
                    # Guardar cookies actualizadas en archivo
                    with open('cookies.json', 'w') as f:
                        json.dump(COOKIES, f, indent=2)
                    
                    print(f"✅ [{discord.utils.utcnow().strftime('%H:%M:%S')}] AccessToken renovado automáticamente")
                    return True
                elif response.status == 401:
                    print(f"⚠️ [{discord.utils.utcnow().strftime('%H:%M:%S')}] RefreshToken expirado - necesitas actualizar las cookies manualmente")
                    # Notificar al admin
                    try:
                        admin_user = await bot.fetch_user(ADMIN_USER_ID)
                        embed = discord.Embed(
                            title="🔑 Sesión Expirada",
                            description="El RefreshToken ha expirado. Necesitas actualizar las cookies.",
                            color=discord.Color.red()
                        )
                        embed.add_field(
                            name="Pasos",
                            value="1. Inicia sesión en anticheat.ac\n2. Ejecuta `py manual_cookies.py`\n3. Usa `/updatecookies` en Discord",
                            inline=False
                        )
                        await admin_user.send(embed=embed)
                    except Exception as e:
                        print(f"⚠️ No se pudo notificar al admin: {e}")
                    return False
                else:
                    print(f"⚠️ Error al renovar token: {response.status}")
                    return False
    except Exception as e:
        print(f"⚠️ Error en refresh_access_token: {e}")
        return False
    """
    Obtiene los resultados completos de un pin escaneado
    """
    try:
        async with aiohttp.ClientSession() as session:
            # Convertir cookies a string de cookie
            cookie_string = "; ".join([f"{key}={value}" for key, value in COOKIES.items()])
            headers_with_cookies = HEADERS.copy()
            headers_with_cookies["cookie"] = cookie_string
            
            # URL para obtener resultados
            results_url = f"https://anticheat.ac/api/pins/{pin_code}"
            
            # Realizar la petición GET
            async with session.get(
                results_url,
                headers=headers_with_cookies,
                timeout=aiohttp.ClientTimeout(total=30)
            ) as response:
                if response.status == 200:
                    data = await response.json()
                    return {"success": True, "data": data, "status": response.status}
                else:
                    error_text = await response.text()
                    return {"success": False, "error": error_text, "status": response.status}
    except Exception as e:
        return {"success": False, "error": str(e), "status": None}

async def crear_pin_api(game_type="Java", pin_name=None, ram_dump=False, private=True):
    """
    Realiza una petición POST a la API de Ocean Anticheat para crear un pin.
    """
    cookie_string = "; ".join([f"{key}={value}" for key, value in COOKIES.items()])
    headers_with_cookies = HEADERS.copy()
    headers_with_cookies["cookie"] = cookie_string

    body = [{
        "type": game_type,
        "pinName": pin_name if pin_name else "",
        "isPrivate": private,
        "ruin": ram_dump,
        "hard": False
    }]

    try:
        async with aiohttp.ClientSession() as session:
            async with session.post(
                DASHBOARD_API_URL,
                headers=headers_with_cookies,
                json=body,
                timeout=aiohttp.ClientTimeout(total=30)
            ) as resp:
                status = resp.status
                text_data = await _read_response_text(resp)

        if status == 200:
            try:
                import re
                json_match = re.search(r'\{"success":true.*?\}\}', text_data)
                if json_match:
                    data = json.loads(json_match.group(0))
                    return {"success": True, "data": data, "status": status}
                else:
                    return {"success": False, "error": f"No se pudo parsear la respuesta: {text_data[:200]}", "status": status}
            except Exception as parse_error:
                return {"success": False, "error": f"Error al parsear respuesta: {str(parse_error)}", "status": status}
        elif status in (401, 403):
            if "Session expired" in text_data or "expired" in text_data.lower():
                return {"success": False, "error": "COOKIES_EXPIRED", "status": status, "message": text_data}
            return {"success": False, "error": text_data, "status": status}
        else:
            return {"success": False, "error": text_data, "status": status}
    except Exception as e:
        return {"success": False, "error": str(e), "status": None}

@bot.event
async def on_ready():
    print(f'Bot conectado como {bot.user}')
    print(f'ID del bot: {bot.user.id}')
    print('------')
    
    # Iniciar tarea de actualización automática de cookies
    actualizar_cookies_periodicamente.start()

# Tarea en segundo plano para actualizar cookies cada 10 minutos
from discord.ext import tasks

@tasks.loop(minutes=10)
async def actualizar_cookies_periodicamente():
    """
    Renueva el accessToken automáticamente cada 10 minutos
    """
    global COOKIES
    try:
        # Intentar renovar el accessToken usando el refreshToken
        success = await refresh_access_token()
        if not success:
            # Si falla, intentar cargar desde Chrome
            try:
                cookies = browser_cookie3.chrome(domain_name='anticheat.ac')
                cookie_dict = {}
                for cookie in cookies:
                    cookie_dict[cookie.name] = cookie.value
                
                if cookie_dict and len(cookie_dict) > 0:
                    COOKIES = cookie_dict
                    print(f"✅ [{discord.utils.utcnow().strftime('%H:%M:%S')}] Cookies actualizadas desde Chrome")
            except Exception as e:
                print(f"⚠️ [{discord.utils.utcnow().strftime('%H:%M:%S')}] Error al cargar cookies de Chrome: {e}")
    except Exception as e:
        print(f"⚠️ [{discord.utils.utcnow().strftime('%H:%M:%S')}] Error al actualizar cookies: {e}")

@actualizar_cookies_periodicamente.before_loop
async def before_actualizar_cookies():
    """
    Espera a que el bot esté listo antes de iniciar la tarea
    """
    await bot.wait_until_ready()

def tiene_rol_permitido(interaction: discord.Interaction) -> bool:
    """
    Verifica si el usuario tiene el rol permitido o está en la lista de autorizados
    """
    # Verificar si es el admin
    if interaction.user.id == ADMIN_USER_ID:
        return True
    
    # Verificar si está en la lista de usuarios autorizados
    if interaction.user.id in authorized_users:
        return True
    
    # Verificar si tiene el rol permitido
    if not interaction.guild:
        return False
    
    member = interaction.guild.get_member(interaction.user.id)
    if not member:
        return False
    
    return any(role.id == ALLOWED_ROLE_ID for role in member.roles)

def verificar_rate_limit(user_id: int) -> tuple[bool, int, int]:
    """
    Verifica si el usuario puede crear un pin según el rate limit
    
    Returns:
        (puede_crear, pins_usados, tiempo_restante_minutos)
    """
    # El admin no tiene límite
    if user_id == ADMIN_USER_ID:
        return (True, 0, 0)
    
    # Verificar si tiene pins extra
    if extra_pins[user_id] > 0:
        return (True, 0, 0)
    
    now = datetime.utcnow()
    cutoff_time = now - timedelta(hours=RATE_LIMIT_HOURS)
    
    # Filtrar solo los pins creados en las últimas RATE_LIMIT_HOURS horas
    user_pin_history[user_id] = [
        timestamp for timestamp in user_pin_history[user_id]
        if timestamp > cutoff_time
    ]
    
    pins_usados = len(user_pin_history[user_id])
    
    if pins_usados >= MAX_PINS_PER_PERIOD:
        # Calcular tiempo restante hasta que expire el pin más antiguo
        oldest_pin = min(user_pin_history[user_id])
        tiempo_restante = (oldest_pin + timedelta(hours=RATE_LIMIT_HOURS)) - now
        minutos_restantes = int(tiempo_restante.total_seconds() / 60)
        return (False, pins_usados, minutos_restantes)
    
    return (True, pins_usados, 0)

@bot.tree.command(name="addperms", description="Dar permisos a un usuario para usar el bot (solo admin)")
@app_commands.describe(
    usuario="Usuario al que dar permisos"
)
async def addperms(interaction: discord.Interaction, usuario: discord.User):
    """
    Comando para dar permisos a un usuario específico
    """
    # Solo el admin puede usar este comando
    if interaction.user.id != ADMIN_USER_ID:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador del bot puede usar este comando.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Agregar usuario a la lista de autorizados
    authorized_users.add(usuario.id)
    
    embed = discord.Embed(
        title="✅ Permisos Otorgados",
        description=f"El usuario {usuario.mention} ahora puede usar el bot.",
        color=discord.Color.green()
    )
    embed.add_field(name="Usuario ID", value=f"`{usuario.id}`", inline=True)
    embed.add_field(name="Total Autorizados", value=f"`{len(authorized_users)}`", inline=True)
    embed.set_footer(text="Roblox Scanner")
    
    await interaction.response.send_message(embed=embed, ephemeral=False)

@bot.tree.command(name="removeperms", description="Quitar permisos a un usuario (solo admin)")
@app_commands.describe(
    usuario="Usuario al que quitar permisos"
)
async def removeperms(interaction: discord.Interaction, usuario: discord.User):
    """
    Comando para quitar permisos a un usuario específico
    """
    # Solo el admin puede usar este comando
    if interaction.user.id != ADMIN_USER_ID:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador del bot puede usar este comando.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Remover usuario de la lista de autorizados
    if usuario.id in authorized_users:
        authorized_users.remove(usuario.id)
        
        embed = discord.Embed(
            title="✅ Permisos Revocados",
            description=f"El usuario {usuario.mention} ya no puede usar el bot.",
            color=discord.Color.orange()
        )
        embed.add_field(name="Usuario ID", value=f"`{usuario.id}`", inline=True)
        embed.add_field(name="Total Autorizados", value=f"`{len(authorized_users)}`", inline=True)
        embed.set_footer(text="Roblox Scanner")
    else:
        embed = discord.Embed(
            title="⚠️ Usuario No Autorizado",
            description=f"El usuario {usuario.mention} no estaba en la lista de autorizados.",
            color=discord.Color.yellow()
        )
    
    await interaction.response.send_message(embed=embed, ephemeral=False)

@bot.tree.command(name="listperms", description="Ver lista de usuarios autorizados (solo admin)")
async def listperms(interaction: discord.Interaction):
    """
    Comando para ver la lista de usuarios autorizados
    """
    # Solo el admin puede usar este comando
    if interaction.user.id != ADMIN_USER_ID:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador del bot puede usar este comando.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    embed = discord.Embed(
        title="📋 Usuarios Autorizados",
        description=f"Total de usuarios con permisos: **{len(authorized_users)}**",
        color=discord.Color.blue()
    )
    
    if authorized_users:
        users_list = []
        for user_id in authorized_users:
            user = await bot.fetch_user(user_id)
            users_list.append(f"• {user.mention} (`{user_id}`)")
        
        embed.add_field(
            name="Usuarios",
            value="\n".join(users_list) if users_list else "Ninguno",
            inline=False
        )
    else:
        embed.add_field(name="Usuarios", value="No hay usuarios autorizados", inline=False)
    
    embed.set_footer(text="Roblox Scanner")
    
    await interaction.response.send_message(embed=embed, ephemeral=False)

@bot.tree.command(name="addmorepins", description="Dar pins extra a un usuario (solo admin)")
@app_commands.describe(
    usuario="Usuario al que dar pins extra",
    cantidad="Cantidad de pins extra a agregar"
)
async def addmorepins(interaction: discord.Interaction, usuario: discord.User, cantidad: int):
    """
    Comando para dar pins extra a un usuario específico
    """
    # Solo el admin puede usar este comando
    if interaction.user.id != ADMIN_USER_ID:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador del bot puede usar este comando.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Validar cantidad
    if cantidad <= 0:
        embed = discord.Embed(
            title="❌ Cantidad Inválida",
            description="La cantidad debe ser mayor a 0.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Agregar pins extra al usuario
    extra_pins[usuario.id] += cantidad
    
    embed = discord.Embed(
        title="🎁 Pins Extra Agregados",
        description=f"Se han agregado **{cantidad} pins extra** a {usuario.mention}.",
        color=discord.Color.green()
    )
    embed.add_field(name="Usuario", value=f"{usuario.mention}", inline=True)
    embed.add_field(name="Pins Extra Totales", value=f"`{extra_pins[usuario.id]}`", inline=True)
    embed.set_footer(text="Roblox Scanner • Los pins extra no expiran")
    
    await interaction.response.send_message(embed=embed, ephemeral=False)

@bot.tree.command(name="checkpins", description="Ver cuántos pins te quedan disponibles")
async def checkpins(interaction: discord.Interaction):
    """
    Comando para que los usuarios vean cuántos pins les quedan
    """
    # Verificar si el usuario tiene el rol permitido
    if not tiene_rol_permitido(interaction):
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="No tienes permiso para usar este comando.",
            color=discord.Color.red()
        )
        embed.set_footer(text="Necesitas el rol autorizado para usar este bot")
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Si es admin
    if interaction.user.id == ADMIN_USER_ID:
        embed = discord.Embed(
            title="👑 Admin - Pins Ilimitados",
            description="Como administrador, tienes **pins ilimitados**.",
            color=discord.Color.gold()
        )
        embed.set_footer(text="Roblox Scanner")
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Verificar pins extra
    pins_extra_disponibles = extra_pins[interaction.user.id]
    
    # Calcular pins normales disponibles
    now = datetime.utcnow()
    cutoff_time = now - timedelta(hours=RATE_LIMIT_HOURS)
    user_pin_history[interaction.user.id] = [
        timestamp for timestamp in user_pin_history[interaction.user.id]
        if timestamp > cutoff_time
    ]
    pins_usados = len(user_pin_history[interaction.user.id])
    pins_normales_disponibles = MAX_PINS_PER_PERIOD - pins_usados
    
    embed = discord.Embed(
        title="📊 Tus Pins Disponibles",
        description=f"Información sobre tus pins disponibles:",
        color=discord.Color.blue()
    )
    
    if pins_extra_disponibles > 0:
        embed.add_field(
            name="🎁 Pins Extra",
            value=f"`{pins_extra_disponibles}` pins extra (no expiran)",
            inline=False
        )
    
    embed.add_field(
        name="📅 Pins Normales",
        value=f"`{pins_normales_disponibles}/{MAX_PINS_PER_PERIOD}` disponibles",
        inline=True
    )
    
    if pins_usados > 0 and pins_normales_disponibles == 0:
        # Calcular tiempo restante
        oldest_pin = min(user_pin_history[interaction.user.id])
        tiempo_restante = (oldest_pin + timedelta(hours=RATE_LIMIT_HOURS)) - now
        horas = int(tiempo_restante.total_seconds() // 3600)
        minutos = int((tiempo_restante.total_seconds() % 3600) // 60)
        
        embed.add_field(
            name="⏰ Próximo Pin Disponible",
            value=f"En **{horas}h {minutos}m**",
            inline=True
        )
    
    embed.set_footer(text=f"Roblox Scanner • Los pins normales se reinician cada {RATE_LIMIT_HOURS}h")
    
    await interaction.response.send_message(embed=embed, ephemeral=False)

@bot.tree.command(name="crearpin", description="Crea un nuevo pin de escaneo para Roblox")
@app_commands.describe(
    pin_name="Nombre personalizado para el pin (opcional)",
    private="Pin privado (solo visible para ti)"
)
async def crearpin(
    interaction: discord.Interaction,
    pin_name: str = None,
    private: bool = True
):
    """
    Comando slash para crear un pin (internamente usa Java, muestra Roblox)
    """
    # Verificar si el usuario tiene el rol permitido
    if not tiene_rol_permitido(interaction):
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="No tienes permiso para usar este comando.",
            color=discord.Color.red()
        )
        embed.set_footer(text="Necesitas el rol autorizado para usar este bot")
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Verificar rate limit (excepto para admin)
    puede_crear, pins_usados, minutos_restantes = verificar_rate_limit(interaction.user.id)
    
    if not puede_crear:
        horas = minutos_restantes // 60
        minutos = minutos_restantes % 60
        
        embed = discord.Embed(
            title="⏱️ Límite de Pins Alcanzado",
            description=f"Has alcanzado el límite de **{MAX_PINS_PER_PERIOD} pins cada {RATE_LIMIT_HOURS} horas**.",
            color=discord.Color.orange()
        )
        embed.add_field(
            name="Tiempo Restante",
            value=f"Podrás crear otro pin en **{horas}h {minutos}m**",
            inline=False
        )
        embed.add_field(
            name="Pins Usados",
            value=f"{pins_usados}/{MAX_PINS_PER_PERIOD}",
            inline=True
        )
        embed.set_footer(text="Roblox Scanner • Rate Limit")
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Defer la respuesta porque la petición puede tardar
    await interaction.response.defer(ephemeral=False)
    
    try:
        # Llamar a la API con Java pero mostrar Roblox
        resultado = await crear_pin_api(
            game_type="Java",  # Internamente usa Java
            pin_name=pin_name,
            ram_dump=False,
            private=private
        )
        
        # Verificar si las cookies expiraron
        if not resultado["success"] and resultado.get("error") == "COOKIES_EXPIRED":
            # Notificar al admin por DM
            try:
                admin_user = await bot.fetch_user(ADMIN_USER_ID)
                embed_admin = discord.Embed(
                    title="🔑 Cookies Expiradas",
                    description="Las cookies de autenticación han expirado. Necesitas actualizarlas.",
                    color=discord.Color.red()
                )
                embed_admin.add_field(
                    name="Pasos para actualizar",
                    value="1. Ejecuta `py manual_cookies.py`\n2. Copia las cookies desde Brave\n3. Sube el nuevo `cookies.json` al servidor\n4. Reinicia el bot",
                    inline=False
                )
                embed_admin.set_footer(text="Tracked Scanner Bot")
                await admin_user.send(embed=embed_admin)
            except Exception as e:
                print(f"⚠️ No se pudo enviar DM al admin: {e}")
            
            # Responder al usuario
            embed = discord.Embed(
                title="❌ Error de Autenticación",
                description="Las cookies de autenticación han expirado. El administrador ha sido notificado.",
                color=discord.Color.red()
            )
            embed.set_footer(text="Roblox Scanner")
            await interaction.followup.send(embed=embed, ephemeral=False)
            return
        
        if resultado["success"]:
            data = resultado["data"]
            
            # Verificar si la respuesta tiene el formato esperado
            if "data" in data and "pin" in data["data"]:
                pin_data = data["data"]
            elif "pin" in data:
                pin_data = data
            else:
                # Si no tiene el formato esperado, mostrar la respuesta completa
                embed = discord.Embed(
                    title="⚠️ Respuesta Inesperada",
                    description="La API devolvió una respuesta en formato inesperado.",
                    color=discord.Color.yellow()
                )
                embed.add_field(name="Respuesta", value=f"```json\n{json.dumps(data, indent=2)[:1000]}```", inline=False)
                await interaction.followup.send(embed=embed, ephemeral=False)
                return
            
            # Registrar el pin en el historial del usuario (solo si no es admin)
            if interaction.user.id != ADMIN_USER_ID:
                # Si tiene pins extra, usar uno de esos
                if extra_pins[interaction.user.id] > 0:
                    extra_pins[interaction.user.id] -= 1
                    print(f"✅ Usuario {interaction.user.id} usó un pin extra. Restantes: {extra_pins[interaction.user.id]}")
                else:
                    user_pin_history[interaction.user.id].append(datetime.utcnow())
            
            # Agregar usuario automáticamente al pin
            if '_id' in pin_data and AUTO_ADD_USER_ID:
                try:
                    add_result = await agregar_usuario_a_pin(pin_data['_id'], AUTO_ADD_USER_ID)
                    if add_result["success"]:
                        print(f"✅ Usuario {AUTO_ADD_USERNAME} agregado automáticamente al pin {pin_data['pin']}")
                    else:
                        print(f"⚠️ No se pudo agregar usuario automáticamente: {add_result.get('error', 'Error desconocido')}")
                except Exception as e:
                    print(f"⚠️ Error al agregar usuario: {e}")
            
            # Calcular pins restantes (solo para usuarios normales)
            pins_restantes = MAX_PINS_PER_PERIOD - (pins_usados + 1)
            
            # Crear embed de éxito
            embed = discord.Embed(
                title="✅ Pin Creado Exitosamente",
                description=f"**PIN: `{pin_data['pin']}`**\n\n**[🔗 Descargar Tracked Scanner](https://anticheat.ac/dl/ocean/{pin_data['pin']})**\n\nComparte este código con el jugador para que inicie el escaneo.",
                color=discord.Color.green()
            )
            
            # Mostrar Roblox visualmente aunque internamente sea Java
            embed.add_field(name="🎮 Tipo de Juego", value="`Roblox`", inline=True)
            embed.add_field(name="📊 Estado", value=f"`{pin_data.get('status', 'pending')}`", inline=True)
            embed.add_field(name="🔒 Privado", value="✅ Sí" if pin_data.get('private', False) else "❌ No", inline=True)
            
            if pin_data.get('pinName'):
                embed.add_field(name="📝 Nombre", value=f"`{pin_data['pinName']}`", inline=True)
            
            # Mostrar pins restantes solo si no es admin
            if interaction.user.id != ADMIN_USER_ID:
                # Calcular pins disponibles
                pins_extra_disponibles = extra_pins[interaction.user.id]
                if pins_extra_disponibles > 0:
                    embed.add_field(
                        name="🎁 Pins Extra",
                        value=f"`{pins_extra_disponibles}` pins extra disponibles",
                        inline=False
                    )
                else:
                    embed.add_field(
                        name="📊 Pins Restantes",
                        value=f"`{pins_restantes}/{MAX_PINS_PER_PERIOD}` (se reinicia en {RATE_LIMIT_HOURS}h)",
                        inline=False
                    )
            
            if 'expiresAt' in pin_data:
                try:
                    embed.add_field(
                        name="⏰ Expira en",
                        value=f"<t:{int(discord.utils.parse_time(pin_data['expiresAt']).timestamp())}:R>",
                        inline=False
                    )
                except:
                    embed.add_field(name="⏰ Expira", value=pin_data['expiresAt'], inline=False)
            
            embed.set_footer(text="Roblox Scanner • El pin expira en ~3 horas")
            embed.timestamp = discord.utils.utcnow()
            
            await interaction.followup.send(embed=embed, ephemeral=False)
        else:
            # Crear embed de error
            embed = discord.Embed(
                title="❌ Error al Crear Pin",
                description="Hubo un problema al intentar crear el pin.",
                color=discord.Color.red()
            )
            
            error_msg = resultado.get('error', 'Error desconocido')
            
            # Mensajes de error más amigables
            if resultado.get('status') == 400:
                error_msg = "❌ Solicitud inválida. Verifica que no tengas más de 3 pins activos."
            elif resultado.get('status') == 403:
                error_msg = "❌ Acceso prohibido. Verifica que tu cuenta no esté baneada y que las cookies sean válidas."
            elif resultado.get('status') == 404:
                error_msg = "❌ Usuario no encontrado. Verifica la configuración de la API."
            
            embed.add_field(name="Error", value=error_msg, inline=False)
            
            if resultado.get('status'):
                embed.add_field(name="Código de Estado", value=f"`{resultado['status']}`", inline=True)
            
            embed.set_footer(text="Roblox Scanner")
            
            await interaction.followup.send(embed=embed, ephemeral=False)
    
    except Exception as e:
        # Error inesperado
        embed = discord.Embed(
            title="❌ Error Inesperado",
            description=f"Ocurrió un error inesperado: ```{str(e)}```",
            color=discord.Color.red()
        )
        await interaction.followup.send(embed=embed, ephemeral=False)

@bot.tree.command(name="crearpinadvanced", description="Crea un nuevo pin de escaneo para Roblox+ (nativo)")
@app_commands.describe(
    pin_name="Nombre personalizado para el pin (opcional)",
    private="Pin privado (solo visible para ti)"
)
async def crearpinadvanced(
    interaction: discord.Interaction,
    pin_name: str = None,
    private: bool = True
):
    """
    Comando slash para crear un pin con Roblox nativo (muestra Roblox+)
    """
    # Verificar si el usuario tiene permisos advanced
    if interaction.user.id != ADMIN_USER_ID and interaction.user.id not in authorized_advanced_users:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="No tienes permiso para usar este comando avanzado.",
            color=discord.Color.red()
        )
        embed.set_footer(text="Necesitas permisos avanzados para usar Roblox+")
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Verificar rate limit (excepto para admin)
    puede_crear, pins_usados, minutos_restantes = verificar_rate_limit(interaction.user.id)
    
    if not puede_crear:
        horas = minutos_restantes // 60
        minutos = minutos_restantes % 60
        
        embed = discord.Embed(
            title="⏱️ Límite de Pins Alcanzado",
            description=f"Has alcanzado el límite de **{MAX_PINS_PER_PERIOD} pins cada {RATE_LIMIT_HOURS} horas**.",
            color=discord.Color.orange()
        )
        embed.add_field(
            name="Tiempo Restante",
            value=f"Podrás crear otro pin en **{horas}h {minutos}m**",
            inline=False
        )
        embed.add_field(
            name="Pins Usados",
            value=f"{pins_usados}/{MAX_PINS_PER_PERIOD}",
            inline=True
        )
        embed.set_footer(text="Roblox Scanner • Rate Limit")
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Defer la respuesta porque la petición puede tardar
    await interaction.response.defer(ephemeral=False)
    
    try:
        # Llamar a la API con Roblox nativo
        resultado = await crear_pin_api(
            game_type="Roblox",  # Usa Roblox nativo
            pin_name=pin_name,
            ram_dump=False,
            private=private
        )
        
        # Verificar si las cookies expiraron
        if not resultado["success"] and resultado.get("error") == "COOKIES_EXPIRED":
            # Notificar al admin por DM
            try:
                admin_user = await bot.fetch_user(ADMIN_USER_ID)
                embed_admin = discord.Embed(
                    title="🔑 Cookies Expiradas",
                    description="Las cookies de autenticación han expirado. Necesitas actualizarlas.",
                    color=discord.Color.red()
                )
                embed_admin.add_field(
                    name="Pasos para actualizar",
                    value="1. Ejecuta `py manual_cookies.py`\n2. Copia las cookies desde Brave\n3. Sube el nuevo `cookies.json` al servidor\n4. Reinicia el bot",
                    inline=False
                )
                embed_admin.set_footer(text="Tracked Scanner Bot")
                await admin_user.send(embed=embed_admin)
            except Exception as e:
                print(f"⚠️ No se pudo enviar DM al admin: {e}")
            
            # Responder al usuario
            embed = discord.Embed(
                title="❌ Error de Autenticación",
                description="Las cookies de autenticación han expirado. El administrador ha sido notificado.",
                color=discord.Color.red()
            )
            embed.set_footer(text="Roblox Scanner")
            await interaction.followup.send(embed=embed, ephemeral=False)
            return
        
        if resultado["success"]:
            data = resultado["data"]
            
            # Verificar si la respuesta tiene el formato esperado
            if "data" in data and "pin" in data["data"]:
                pin_data = data["data"]
            elif "pin" in data:
                pin_data = data
            else:
                # Si no tiene el formato esperado, mostrar la respuesta completa
                embed = discord.Embed(
                    title="⚠️ Respuesta Inesperada",
                    description="La API devolvió una respuesta en formato inesperado.",
                    color=discord.Color.yellow()
                )
                embed.add_field(name="Respuesta", value=f"```json\n{json.dumps(data, indent=2)[:1000]}```", inline=False)
                await interaction.followup.send(embed=embed, ephemeral=False)
                return
            
            # Registrar el pin en el historial del usuario (solo si no es admin)
            if interaction.user.id != ADMIN_USER_ID:
                # Si tiene pins extra, usar uno de esos
                if extra_pins[interaction.user.id] > 0:
                    extra_pins[interaction.user.id] -= 1
                    print(f"✅ Usuario {interaction.user.id} usó un pin extra. Restantes: {extra_pins[interaction.user.id]}")
                else:
                    user_pin_history[interaction.user.id].append(datetime.utcnow())
            
            # Agregar usuario automáticamente al pin
            if '_id' in pin_data and AUTO_ADD_USER_ID:
                try:
                    add_result = await agregar_usuario_a_pin(pin_data['_id'], AUTO_ADD_USER_ID)
                    if add_result["success"]:
                        print(f"✅ Usuario {AUTO_ADD_USERNAME} agregado automáticamente al pin {pin_data['pin']}")
                    else:
                        print(f"⚠️ No se pudo agregar usuario automáticamente: {add_result.get('error', 'Error desconocido')}")
                except Exception as e:
                    print(f"⚠️ Error al agregar usuario: {e}")
            
            # Calcular pins restantes (solo para usuarios normales)
            pins_restantes = MAX_PINS_PER_PERIOD - (pins_usados + 1)
            
            # Crear embed de éxito
            embed = discord.Embed(
                title="✅ Pin Creado Exitosamente",
                description=f"**PIN: `{pin_data['pin']}`**\n\n**[🔗 Descargar Tracked Scanner](https://anticheat.ac/dl/ocean/{pin_data['pin']})**\n\nComparte este código con el jugador para que inicie el escaneo.",
                color=discord.Color.purple()
            )
            
            # Mostrar Roblox+ para indicar que es nativo
            embed.add_field(name="🎮 Tipo de Juego", value="`Roblox+`", inline=True)
            embed.add_field(name="📊 Estado", value=f"`{pin_data.get('status', 'pending')}`", inline=True)
            embed.add_field(name="🔒 Privado", value="✅ Sí" if pin_data.get('private', False) else "❌ No", inline=True)
            
            if pin_data.get('pinName'):
                embed.add_field(name="📝 Nombre", value=f"`{pin_data['pinName']}`", inline=True)
            
            # Mostrar pins restantes solo si no es admin
            if interaction.user.id != ADMIN_USER_ID:
                # Calcular pins disponibles
                pins_extra_disponibles = extra_pins[interaction.user.id]
                if pins_extra_disponibles > 0:
                    embed.add_field(
                        name="🎁 Pins Extra",
                        value=f"`{pins_extra_disponibles}` pins extra disponibles",
                        inline=False
                    )
                else:
                    embed.add_field(
                        name="📊 Pins Restantes",
                        value=f"`{pins_restantes}/{MAX_PINS_PER_PERIOD}` (se reinicia en {RATE_LIMIT_HOURS}h)",
                        inline=False
                    )
            
            if 'expiresAt' in pin_data:
                try:
                    embed.add_field(
                        name="⏰ Expira en",
                        value=f"<t:{int(discord.utils.parse_time(pin_data['expiresAt']).timestamp())}:R>",
                        inline=False
                    )
                except:
                    embed.add_field(name="⏰ Expira", value=pin_data['expiresAt'], inline=False)
            
            embed.set_footer(text="Roblox Scanner • Roblox+ (Nativo) • El pin expira en ~3 horas")
            embed.timestamp = discord.utils.utcnow()
            
            await interaction.followup.send(embed=embed, ephemeral=False)
        else:
            # Crear embed de error
            embed = discord.Embed(
                title="❌ Error al Crear Pin",
                description="Hubo un problema al intentar crear el pin.",
                color=discord.Color.red()
            )
            
            error_msg = resultado.get('error', 'Error desconocido')
            
            # Mensajes de error más amigables
            if resultado.get('status') == 400:
                error_msg = "❌ Solicitud inválida. Verifica que no tengas más de 3 pins activos."
            elif resultado.get('status') == 403:
                error_msg = "❌ Acceso prohibido. Verifica que tu cuenta no esté baneada y que las cookies sean válidas."
            elif resultado.get('status') == 404:
                error_msg = "❌ Usuario no encontrado. Verifica la configuración de la API."
            
            embed.add_field(name="Error", value=error_msg, inline=False)
            
            if resultado.get('status'):
                embed.add_field(name="Código de Estado", value=f"`{resultado['status']}`", inline=True)
            
            embed.set_footer(text="Roblox Scanner")
            
            await interaction.followup.send(embed=embed, ephemeral=False)
    
    except Exception as e:
        # Error inesperado
        embed = discord.Embed(
            title="❌ Error Inesperado",
            description=f"Ocurrió un error inesperado: ```{str(e)}```",
            color=discord.Color.red()
        )
        await interaction.followup.send(embed=embed, ephemeral=False)

@bot.tree.command(name="addadvancedperms", description="Dar permisos avanzados (Roblox+) a un usuario (solo admin)")
@app_commands.describe(
    usuario="Usuario al que dar permisos avanzados"
)
async def addadvancedperms(interaction: discord.Interaction, usuario: discord.User):
    """
    Comando para dar permisos avanzados a un usuario específico
    """
    # Solo el admin puede usar este comando
    if interaction.user.id != ADMIN_USER_ID:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador del bot puede usar este comando.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Agregar usuario a la lista de autorizados advanced
    authorized_advanced_users.add(usuario.id)
    
    embed = discord.Embed(
        title="✅ Permisos Avanzados Otorgados",
        description=f"El usuario {usuario.mention} ahora puede usar `/crearpinadvanced` (Roblox+).",
        color=discord.Color.purple()
    )
    embed.add_field(name="Usuario ID", value=f"`{usuario.id}`", inline=True)
    embed.add_field(name="Total Autorizados Advanced", value=f"`{len(authorized_advanced_users)}`", inline=True)
    embed.set_footer(text="Roblox Scanner • Permisos Avanzados")
    
    await interaction.response.send_message(embed=embed, ephemeral=False)

@bot.tree.command(name="removeadvancedperms", description="Quitar permisos avanzados (Roblox+) a un usuario (solo admin)")
@app_commands.describe(
    usuario="Usuario al que quitar permisos avanzados"
)
async def removeadvancedperms(interaction: discord.Interaction, usuario: discord.User):
    """
    Comando para quitar permisos avanzados a un usuario específico
    """
    # Solo el admin puede usar este comando
    if interaction.user.id != ADMIN_USER_ID:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador del bot puede usar este comando.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    # Remover usuario de la lista de autorizados advanced
    if usuario.id in authorized_advanced_users:
        authorized_advanced_users.remove(usuario.id)
        
        embed = discord.Embed(
            title="✅ Permisos Avanzados Revocados",
            description=f"El usuario {usuario.mention} ya no puede usar `/crearpinadvanced` (Roblox+).",
            color=discord.Color.orange()
        )
        embed.add_field(name="Usuario ID", value=f"`{usuario.id}`", inline=True)
        embed.add_field(name="Total Autorizados Advanced", value=f"`{len(authorized_advanced_users)}`", inline=True)
        embed.set_footer(text="Roblox Scanner • Permisos Avanzados")
    else:
        embed = discord.Embed(
            title="⚠️ Usuario No Autorizado",
            description=f"El usuario {usuario.mention} no tenía permisos avanzados.",
            color=discord.Color.yellow()
        )
    
    await interaction.response.send_message(embed=embed, ephemeral=False)

@bot.tree.command(name="listadvancedperms", description="Ver lista de usuarios con permisos avanzados (solo admin)")
async def listadvancedperms(interaction: discord.Interaction):
    """
    Comando para ver la lista de usuarios con permisos avanzados
    """
    # Solo el admin puede usar este comando
    if interaction.user.id != ADMIN_USER_ID:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador del bot puede usar este comando.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=False)
        return
    
    embed = discord.Embed(
        title="📋 Usuarios con Permisos Avanzados",
        description=f"Total de usuarios con permisos Roblox+: **{len(authorized_advanced_users)}**",
        color=discord.Color.purple()
    )
    
    if authorized_advanced_users:
        users_list = []
        for user_id in authorized_advanced_users:
            user = await bot.fetch_user(user_id)
            users_list.append(f"• {user.mention} (`{user_id}`)")
        
        embed.add_field(
            name="Usuarios",
            value="\n".join(users_list) if users_list else "Ninguno",
            inline=False
        )
    else:
        embed.add_field(name="Usuarios", value="No hay usuarios con permisos avanzados", inline=False)
    
    embed.set_footer(text="Roblox Scanner • Permisos Avanzados")
    
    await interaction.response.send_message(embed=embed, ephemeral=False)

@bot.tree.command(name="updatecookies", description="Actualiza las cookies desde un archivo cookies.json (solo admin)")
@app_commands.describe(
    archivo="Archivo cookies.json con las nuevas cookies"
)
async def updatecookies(interaction: discord.Interaction, archivo: discord.Attachment):
    """
    Comando para actualizar las cookies desde un archivo JSON adjunto
    """
    # Solo el admin puede usar este comando
    if interaction.user.id != ADMIN_USER_ID:
        embed = discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador del bot puede usar este comando.",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=True)
        return
    
    # Verificar que sea un archivo JSON
    if not archivo.filename.endswith('.json'):
        embed = discord.Embed(
            title="❌ Archivo Inválido",
            description="El archivo debe ser un archivo `.json`",
            color=discord.Color.red()
        )
        await interaction.response.send_message(embed=embed, ephemeral=True)
        return
    
    # Defer la respuesta
    await interaction.response.defer(ephemeral=True)
    
    try:
        # Descargar el archivo
        file_bytes = await archivo.read()
        file_content = file_bytes.decode('utf-8')
        
        # Parsear el JSON
        new_cookies = json.loads(file_content)
        
        # Validar que tenga las cookies necesarias
        required_cookies = ['accessToken', 'refreshToken', 'fingerprint', 'deviceId']
        missing_cookies = [cookie for cookie in required_cookies if cookie not in new_cookies]
        
        if missing_cookies:
            embed = discord.Embed(
                title="❌ Cookies Incompletas",
                description=f"Faltan las siguientes cookies: `{', '.join(missing_cookies)}`",
                color=discord.Color.red()
            )
            await interaction.followup.send(embed=embed, ephemeral=True)
            return
        
        # Guardar las nuevas cookies en el archivo
        with open('cookies.json', 'w') as f:
            json.dump(new_cookies, f, indent=2)
        
        # Actualizar las cookies globales
        global COOKIES
        COOKIES = new_cookies
        
        embed = discord.Embed(
            title="✅ Cookies Actualizadas",
            description="Las cookies han sido actualizadas correctamente desde el archivo.",
            color=discord.Color.green()
        )
        embed.add_field(name="Cookies Cargadas", value=f"`{len(new_cookies)}` cookies", inline=True)
        embed.add_field(name="Archivo", value=f"`{archivo.filename}`", inline=True)
        embed.set_footer(text="Roblox Scanner • Las cookies se han guardado en cookies.json")
        
        await interaction.followup.send(embed=embed, ephemeral=True)
        
        print(f"✅ Cookies actualizadas por {interaction.user} desde archivo {archivo.filename}")
        
    except json.JSONDecodeError:
        embed = discord.Embed(
            title="❌ Error de Formato",
            description="El archivo no es un JSON válido. Verifica el formato.",
            color=discord.Color.red()
        )
        await interaction.followup.send(embed=embed, ephemeral=True)
    except Exception as e:
        embed = discord.Embed(
            title="❌ Error Inesperado",
            description=f"Ocurrió un error al procesar el archivo: ```{str(e)}```",
            color=discord.Color.red()
        )
        await interaction.followup.send(embed=embed, ephemeral=True)

@bot.tree.command(name="updatehash", description="Actualiza los hashes de Next.js (solo admin)")
@app_commands.describe(
    tipo="Qué hash actualizar",
    hash="El nuevo hash obtenido del cURL"
)
@app_commands.choices(tipo=[
    app_commands.Choice(name="Crear Pin", value="crear_pin"),
    app_commands.Choice(name="Manage Access", value="manage_access"),
])
async def updatehash(interaction: discord.Interaction, tipo: app_commands.Choice[str], hash: str):
    if interaction.user.id != ADMIN_USER_ID:
        await interaction.response.send_message(embed=discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador puede usar este comando.",
            color=discord.Color.red()
        ), ephemeral=True)
        return

    global HASH_CREAR_PIN, HASH_MANAGE_ACCESS, HEADERS

    if tipo.value == "crear_pin":
        HASH_CREAR_PIN = hash
        HEADERS["next-action"] = hash
        nombre = "Crear Pin"
    else:
        HASH_MANAGE_ACCESS = hash
        nombre = "Manage Access"

    embed = discord.Embed(
        title="✅ Hash Actualizado",
        description=f"El hash de **{nombre}** ha sido actualizado.",
        color=discord.Color.green()
    )
    embed.add_field(name="Nuevo Hash", value=f"`{hash}`", inline=False)
    embed.set_footer(text="Roblox Scanner")
    await interaction.response.send_message(embed=embed, ephemeral=True)


@bot.tree.command(name="railway", description="Guía para desplegar el bot en Railway (solo admin)")
async def railway(interaction: discord.Interaction):
    if interaction.user.id != ADMIN_USER_ID:
        await interaction.response.send_message(embed=discord.Embed(
            title="❌ Acceso Denegado",
            description="Solo el administrador puede ver esta guía.",
            color=discord.Color.red()
        ), ephemeral=True)
        return

    embed = discord.Embed(
        title="🚂 Guía de Despliegue en Railway",
        description="Sigue estos pasos para subir el bot a Railway.",
        color=discord.Color.blue()
    )

    embed.add_field(
        name="1️⃣ Preparar GitHub",
        value=(
            "• Crea un repo nuevo en github.com\n"
            "• Sube todos los archivos del bot **excepto** `.env` y `cookies.json`\n"
            "• Asegúrate de que `.gitignore` incluya `.env` y `cookies.json`"
        ),
        inline=False
    )

    embed.add_field(
        name="2️⃣ Crear proyecto en Railway",
        value=(
            "• Ve a railway.app → New Project\n"
            "• Selecciona **Deploy from GitHub repo**\n"
            "• Conecta tu cuenta de GitHub y selecciona el repo"
        ),
        inline=False
    )

    embed.add_field(
        name="3️⃣ Variables de entorno",
        value=(
            "En Railway → tu proyecto → **Variables**, agrega:\n"
            "```\n"
            "DISCORD_TOKEN = tu_token\n"
            "GUILD_ID = tu_guild_id (opcional)\n"
            "```"
        ),
        inline=False
    )

    embed.add_field(
        name="4️⃣ Subir cookies iniciales",
        value=(
            "• El bot arrancará pero necesita cookies para funcionar\n"
            "• Saca las cookies frescas de Chrome con tu script\n"
            "• Usa `/updatecookies` en Discord adjuntando el archivo `cookies.json`"
        ),
        inline=False
    )

    embed.add_field(
        name="5️⃣ Mantenimiento",
        value=(
            "• Las cookies se renuevan automáticamente cada 10 min\n"
            "• Si da error 403 sin razón → el `next-action` hash cambió\n"
            "• Para actualizarlo: F12 en anticheat.ac → crear pin → Copy as cURL → busca `next-action` → actualiza en `HEADERS` del bot"
        ),
        inline=False
    )

    embed.set_footer(text="Roblox Scanner • Railway Deployment Guide")
    await interaction.response.send_message(embed=embed, ephemeral=True)


# Ejecutar el bot
if __name__ == "__main__":
    if not DISCORD_TOKEN:
        print("ERROR: No se encontró el token de Discord. Configura la variable DISCORD_TOKEN en el archivo .env")
    else:
        bot.run(DISCORD_TOKEN)
