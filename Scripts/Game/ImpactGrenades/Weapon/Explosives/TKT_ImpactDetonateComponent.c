class TKT_ImpactDetonateComponentClass : ScriptComponentClass {}

class TKT_ImpactDetonateComponent : ScriptComponent 
{
	[Attribute(defvalue: "0.15", desc: "Seconds after *throw* before impact is armed")]
	protected float m_armingDelay;

	[Attribute(defvalue: "1.2", desc: "Min speed (m/s) to count as an impact")]
	protected float m_minImpactSpeed;

	[Attribute(defvalue: "3.0", desc: "Speed (m/s) that means we’ve actually been thrown")]
	protected float m_throwSpeedGate;

	protected bool m_armed   = false;
	protected bool m_thrown  = false;
	protected bool m_done    = false;

	protected RplComponent         m_rpl;
	protected Physics              m_phys;
	protected BaseTriggerComponent m_trigger; // TimerTriggerComponent derives from this

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_rpl    = TC_Replication.EntRpl(owner);
		m_phys   = owner.GetPhysics();
		m_trigger = BaseTriggerComponent.Cast(owner.FindComponent(BaseTriggerComponent));

		// Listen for contacts AND frames (we use frames to detect the actual throw)
		SetEventMask(owner, EntityEvent.CONTACT | EntityEvent.FRAME);

		Print("[ImpactFuze] OnPostInit: subscribed to CONTACT+FRAME");
	}

	// Wait until the projectile actually leaves the hand (speed gate), THEN arm after delay
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_thrown)
		{
			float s;
			if(m_phys) {
				s = m_phys.GetVelocity().Length();
			} else {
				s = 0.0;
			}
			
			if (s >= m_throwSpeedGate)
			{
				m_thrown = true;
				GetGame().GetCallqueue().CallLater(Arm, Math.Round(m_armingDelay * 1000), false);
				PrintFormat("[ImpactFuze] Thrown detected (%1 m/s), arming scheduled", s);
			}
		}
	}

	protected void Arm()
	{
		m_armed = true;
		Print("[ImpactFuze] Armed");
	}
	
	protected bool MeetsRequirements(bool baseTriggerCheck = true)
	{	
		Print("[ImpactFuze] MeetsRequirements() run");

		// NOTE: braces matter; otherwise you'd always return. Keep them.
		if (m_done || !m_armed)
		{
			Print("[ImpactFuze] Ignored (done or not armed)");
			return false;
		}

		// Authority check for MP determinism
		if (m_rpl && !m_rpl.IsMaster())
		{
			Print("[ImpactFuze] Not master, ignoring");
			return false;
		}

		// Energy gate to ignore brush hits
		float speed;
		if(m_phys) {
			speed = m_phys.GetVelocity().Length();
		} else {
			speed = 0.0;
		}
		if (speed < m_minImpactSpeed)
		{
			PrintFormat("[ImpactFuze] Speed too low: %1 < %2", speed, m_minImpactSpeed);
			return false;
		}

		if (baseTriggerCheck && !m_trigger)
		{
			Print("[ImpactFuze] No BaseTriggerComponent found");
			return false;
		}
		
		return true;
	}

	override void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		Print("[ImpactFuze] EOnContact");
		
		if(!MeetsRequirements()) return;
		
		m_done = true;
		ClearEventMask(owner, EntityEvent.CONTACT | EntityEvent.FRAME);
		
		Print("[ImpactFuze] Detonating via OnUserTrigger()");
		
		m_trigger.OnUserTrigger(owner); // fire the default explosion pipeline
	}
}
