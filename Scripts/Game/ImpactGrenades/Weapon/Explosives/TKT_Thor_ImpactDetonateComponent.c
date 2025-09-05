class TKT_Thor_ImpactDetonateComponentClass :  TKT_ImpactDetonateComponentClass {}

class TKT_Thor_ImpactDetonateComponent : TKT_ImpactDetonateComponent 
{
	
	[Attribute("", UIWidgets.ResourceNamePicker, "Lightning prefab", "et")]
	protected ResourceName m_lightningPrefab;   // pick the GM lightning prefab in WB
	
	[Attribute(defvalue: "2.0", desc: "Lightning visibility radius (km)")]
	protected float m_lightningRadiusKm;
	
	protected void SpawnLightningPrefabAt(vector pos)
	{
		// only the authority should spawn replicated effects
		if (m_rpl && !m_rpl.IsMaster()) return;
		if (!m_lightningPrefab) return;
	
		Resource res = Resource.Load(m_lightningPrefab);
		if (!res) return;
	
		BaseWorld world = GetGame().GetWorld();
		EntitySpawnParams p = new EntitySpawnParams();
		p.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(p.Transform);
		p.Transform[3] = pos;
	
		IEntity boltEnt = GetGame().SpawnEntityPrefab(res, world, p); // replicated if prefab has RplComponent
		// prefab should include SCR_LightningComponent + VFX + (optional) SoundComponent
	}
	
	protected void FlashWeatherAt(vector pos)
	{
		// authority only; clients will get it via weather manager
		if (m_rpl && !m_rpl.IsMaster()) return;
	
		BaseWorld w = GetGame().GetWorld();
		ChimeraWorld cw = ChimeraWorld.CastFrom(w);
		BaseWeatherManagerEntity wm = null;
	
		if (cw) {
			TimeAndWeatherManagerEntity tw = cw.GetTimeAndWeatherManager();
			wm = BaseWeatherManagerEntity.Cast(tw);
		}
		if (!wm) {
			IEntity wmEnt = WeatherManager.GetRegisteredWeatherManagerEntity(w);
			wm = BaseWeatherManagerEntity.Cast(wmEnt);
		}
		if (!wm) return;
	
		WeatherLightning l = new WeatherLightning();
		l.SetPosition(pos);
		l.SetRadius(Math.Max(0.1, m_lightningRadiusKm));
	
		WeatherLightningFlash f1 = new WeatherLightningFlash(); f1.SetStartTime(0.0);  f1.SetDuration(0.15); f1.SetCooldown(0.45);
		WeatherLightningFlash f2 = new WeatherLightningFlash(); f2.SetStartTime(0.35); f2.SetDuration(0.12); f2.SetCooldown(0.0);
		l.AddLightningFlash(f1);
		l.AddLightningFlash(f2);
	
		wm.AddLightning(l);
	}
	
	override void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		Print("[TKT_T_IDC] EOnContact");

		if(!MeetsRequirements()) return;

		m_done = true;
		ClearEventMask(owner, EntityEvent.CONTACT | EntityEvent.FRAME);
		vector hitPos = owner.GetOrigin();
		SpawnLightningPrefabAt(hitPos);  // visible bolt & thunder (if prefab has sound)
		FlashWeatherAt(hitPos);          // global sky flash so everyone gets the exposure pulse
		m_trigger.OnUserTrigger(owner);
	}
}

