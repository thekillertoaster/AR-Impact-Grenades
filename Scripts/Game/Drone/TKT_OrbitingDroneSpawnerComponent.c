class TKT_OrbitingDroneSpawnerComponentClass : ScriptComponentClass {}

class TKT_OrbitingDroneSpawnerComponent : ScriptComponent 
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Drone prefab to spawn", "et")]
	protected ResourceName m_dronePrefab;

	[Attribute(defvalue: "30.0", desc: "Orbit height above anchor (meters)")]
	protected float m_height;

	[Attribute(defvalue: "25.0", desc: "Orbit radius (meters)")]
	protected float m_radius;

	[Attribute(defvalue: "12.0", desc: "Orbit speed (m/s)")]
	protected float m_speed;

	[Attribute(defvalue: "1", desc: "Counter-Clockwise (1) or Clockwise (0)")]
	protected bool m_counterClockwise;

	[Attribute(defvalue: "1.0", desc: "Banking multiplier (0=flat, 1=realistic-ish)")]
	protected float m_bankScale;

	[Attribute(defvalue: "0.5", desc: "Respawn delay if drone is destroyed (s)")]
	protected float m_respawnDelay;

	[Attribute(defvalue: "0.0", desc: "Orbit yaw offset (degrees)")]
	protected float m_yawOffsetDeg;

	// --- state ---
	protected IEntity m_drone;
	protected float   m_theta;
	protected vector  m_prevPos;
	protected bool    m_wasSpawned;

	protected const float G = 9.81;
	protected const float DEG2RAD = 0.017453292519943295; // pi/180

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.FRAME);
		if (IsMaster())
			SpawnDrone();
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);
		if (IsMaster())
			SafeDelete(m_drone);
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!IsMaster()) return;

		if (!m_drone && m_wasSpawned)
		{
			GetGame().GetCallqueue().CallLater(SpawnDrone, Math.Round(m_respawnDelay * 1000), false);
			m_wasSpawned = false;
			return;
		}

		if (!m_drone) return;
		if (m_radius <= 0.01 || m_speed <= 0.01) return;

		vector center = owner.GetOrigin() + Vector(0, m_height, 0);

		float dir;
		if (m_counterClockwise) dir = 1.0;
		else dir = -1.0;

		float omega = (m_speed / m_radius) * dir; // rad/s
		m_theta += omega * timeSlice;

		// yaw plane rotation
		float yawRad = m_yawOffsetDeg * DEG2RAD;
		float cYaw = Math.Cos(yawRad);
		float sYaw = Math.Sin(yawRad);

		float c = Math.Cos(m_theta);
		float s = Math.Sin(m_theta);

		float x =  m_radius * ( c * cYaw - s * sYaw);
		float z =  m_radius * ( c * sYaw + s * cYaw);

		vector pos = center + Vector(x, 0, z);

		// tangent direction (horizontal) after yaw
		float tx = (-s * cYaw - c * sYaw) * dir;
		float tz = (-s * sYaw + c * cYaw) * dir;
		vector forward = Normalize(Vector(tx, 0, tz));

		// bank angle (robust)
		float bank = Math.Atan2(m_speed * m_speed, Math.Max(0.1, (m_radius * G)));
		bank *= m_bankScale;

		// right vector from world up × forward, no Math3D
		vector worldUp  = Vector(0,1,0);
		vector right0   = Normalize(Cross(worldUp, forward));

		// roll toward circle center
		vector toCenter = Normalize(center - pos);
		float dotRC = Dot(right0, toCenter);

		float sign;
		if (dotRC > 0.0) sign = -1.0;
		else sign = 1.0;

		float roll = bank * sign;

		// up0 = forward × right0 (for horizontal forward this is world up)
		vector up0    = Normalize(Cross(forward, right0));
		float  cr     = Math.Cos(roll);
		float  sr     = Math.Sin(roll);
		vector right  = right0 * cr + up0 * sr;
		vector up     = (-right0) * sr + up0 * cr;

		vector tr[4];
		tr[0] = right;
		tr[1] = up;
		tr[2] = forward;
		tr[3] = pos;

		m_drone.SetTransform(tr);

		m_prevPos = pos;
	}

	protected void SpawnDrone()
	{
		if (m_drone || !m_dronePrefab) return;

		Resource res = Resource.Load(m_dronePrefab);
		if (!res) { Print("[TKT_Orbit] failed to load drone prefab"); return; }

		EntitySpawnParams p = new EntitySpawnParams();
		p.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(p.Transform);
		p.Transform[3] = GetOwner().GetOrigin() + Vector(0, m_height, 0);

		m_drone = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), p);
		if (!m_drone) { Print("[TKT_Orbit] spawn returned null"); return; }

		RplComponent rpl = RplComponent.Cast(m_drone.FindComponent(RplComponent));
		if (!rpl) Print("[TKT_Orbit] WARNING: drone has no RplComponent; clients won't see it");

		m_wasSpawned = true;
		m_prevPos = m_drone.GetOrigin();
	}

	// ---------- helpers ----------
	protected bool IsMaster()
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (rpl) return rpl.IsMaster();
		return Replication.IsServer();
	}

	protected static vector Normalize(vector v)
	{
		float len = v.Length();
		if (len <= 0.0001) return "0 0 0";
		return v / len;
	}

	protected static float Dot(vector a, vector b)
	{
		return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
	}

	protected static vector Cross(vector a, vector b)
	{
		// (ay*bz - az*by, az*bx - ax*bz, ax*by - ay*bx)
		return Vector(
			a[1]*b[2] - a[2]*b[1],
			a[2]*b[0] - a[0]*b[2],
			a[0]*b[1] - a[1]*b[0]
		);
	}

	protected void SafeDelete(IEntity ent)
	{
		if (!ent) return;
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (rpl) RplComponent.DeleteRplEntity(ent, true);
	}
}
