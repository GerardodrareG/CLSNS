#include <Rcpp.h> 
//using namespace Rcpp;
// Parallel computation and random numbers
// [[Rcpp::plugins(openmp)]]
#ifdef _OPENMP 
#include <omp.h>
#define thisThread omp_get_thread_num()
#else
#define thisThread 0
#endif
#include <cstdint>
#include <vector> 
#include<iostream>
#include <thread>
// [[Rcpp::depends(sitmo)]]
#include <sitmo.h>
// [[Rcpp::depends(BH)]]
#include <boost/random/normal_distribution.hpp>
#include <boost/random/student_t_distribution.hpp>
#include <boost/random/gamma_distribution.hpp>
#include <boost/math/special_functions.hpp>
#define threadRNG (parallel::rngs.r[thisThread])
namespace parallel {
    struct RNGS {
        uint32_t nthreads, seed;
        std::vector< sitmo::prng_engine * > r;
        RNGS() : nthreads(1), seed(1) { 
            r.push_back(new sitmo::prng_engine(seed));
        }
        ~RNGS() {
            for (uint32_t i = 0; i<r.size(); i++) delete r[i];
        }
        void setPThreads(uint32_t nt) {
#ifdef _OPENMP
            nthreads = nt;
            for (uint32_t i=r.size(); i<nthreads; i++) {
	        uint32_t thisSeed = (seed+i) % sitmo::prng_engine::max();
	        r.push_back(new sitmo::prng_engine(thisSeed));
            }
#else
            Rcpp::warning("No openmp support");
#endif
        }
        void setPSeed(double s) {
            if ((s<=0.0) || (s>=1.0)) 
                Rcpp::stop("seed must be between 0 and 1");
            seed = s * sitmo::prng_engine::max();
            for (uint32_t i=0; i<r.size(); i++) {
	        uint32_t thisSeed = (seed+i) % sitmo::prng_engine::max();
	        r[i]->seed(thisSeed);
            }
        }

    };

    RNGS rngs;

}

#include <cmath>
#include <random> 
#include <vector> 
#include <string>

double quantile(std::vector<double> data, double p) {
  int n = data.size();
  sort(data.begin(), data.end()); // Ordenar la muestra
  int k = static_cast<int>(p * n); // Índice del cuantil
  if (n == 0 || k <= 0 || k >= n) {
    // La muestra esta vacia o el cuantil esta fuera de rango
    return std::numeric_limits<double>::quiet_NaN();
  } else {
    // Calcular el cuantil sin interpolacion
    return data[k - 1];
  }
}

double media(std::vector<double> Y) {
    double sum = 0.0;
    int m = Y.size();
    for (int i = 0; i < m; i++) {
            sum += Y[i];
    }
    double media = sum / m ;
    return media;
}


double var(std::vector<double> Y, double m) {
    double sum = 0.0;
    int m_size = Y.size(); 
    for (int i = 0; i < m_size; i++) {
            sum += pow(Y[i] - m, 2.0);
    }
    double s2= sum / (m_size - 1);
    return s2;
}
//Normal Scores para datos agrupados con cuantil conocido o desconocido
std::vector<double> Normal_S(std::vector<double> muestra, double theta, double Ftheta) {
  int n = muestra.size();
  std::vector<double> rangos(n);
  std::vector<double> prob(n);
  std::vector<int> menores;
  std::vector<int> mayores;
  std::vector<double> Menores;
  std::vector<double> Mayores; 
  for (int i = 0; i < n; i++) {
    if (muestra[i] <= theta) {
      menores.push_back(i);
       Menores.push_back(muestra[i]);
    } else {
      mayores.push_back(i);
      Mayores.push_back(muestra[i]);
    }
  }
  int n_men = menores.size();
  int n_may = mayores.size();
  
  if(n_men>0){   
  for (int i = 0; i < n_men; i++) {
    int index = menores[i];
    rangos[index] = (double) (count_if(Menores.begin(), Menores.end(), [=](double x){ return x < muestra[index]; }) + 1);
  }
}
if(n_may>0){
  for (int i = 0; i < n_may; i++) {
    int index = mayores[i];
    rangos[index] = (double) (count_if(Mayores.begin(), Mayores.end(), [=](double x){ return x < muestra[index]; }) + 1);
  }
}
if(n_men>0){
  for (int i = 0; i < n_men; i++) { 
    int index = menores[i];
    prob[index] = Ftheta * (rangos[index] - 0.5) / (double) (menores.size() );
  }
}
if(n_may>0){
  for (int i = 0; i < n_may; i++) {
    int index = mayores[i];
    prob[index] = Ftheta + (1 - Ftheta) * (rangos[index] - 0.5) / (double) (mayores.size() );
  }
}

  std::vector<double> Zs(n);
  for (int i = 0; i < n; i++) {
    Zs[i] = std::sqrt(2) * boost::math::erf_inv(2 * prob[i] - 1);
  }
  return Zs;
}
//Funcion SNS para datos agrupados con cuantil conocido o desconocido
double Seq_NS_l(std::vector<double> X, std::vector<double> Y, double theta, double Ftheta) {
  int n = X.size();
  int m= Y.size();
  std::vector<double> rangos(n);
  std::vector<double> prob(n);
  std::vector<double> Menores;
  std::vector<double> Mayores;
  for (int i = 0; i < m; i++) {
  if (Y[i] <= theta) {
        Menores.push_back(Y[i]);
      } else {
        Mayores.push_back(Y[i]);
      }
  }
    std::vector<int> menores;
    std::vector<int> mayores;
    n = X.size();
  for (int i = 0; i < n; i++) {
      if (X[i] <= theta) {
        menores.push_back(i);
      } else {
        mayores.push_back(i);
      }
	  }
    int n_menores = Menores.size();
    int n_mayores = Mayores.size();
    int n_men_x = menores.size();
    int n_may_x = mayores.size();
    if(n_men_x>0){
    for (int j = 0; j < n_men_x; j++) {
      int k = menores[j];
      int contador = 0;
      for (int l = 0; l < n_menores; l++) {
        if (Menores[l] < X[k]) {
          contador++;
        }
      }
      	rangos[k] =  contador+1;
      	prob[k] = Ftheta * (rangos[k] - 0.5) / (n_menores+1 );  
    }
}
if(n_may_x>0){
    for (int j = 0; j < n_may_x; j++) {
      int k = mayores[j];
      int contador = 0;
      for (int l = 0; l < n_mayores; l++) {
        if (Mayores[l] < X[k]) {
          contador++;
        } 
      }
        rangos[k] =contador+1;
      	prob[k] = Ftheta+(1-Ftheta) * (rangos[k] - 0.5) / (n_mayores+1);
    }
}
    double Z=0;
  for (int i = 0; i < n; i++) {
            Z+=  (std::sqrt(2) * boost::math::erf_inv(2 * prob[i] - 1))/ std::sqrt(n);
  } 
  return Z;
}
namespace {

    // a struct for simulating x, xbarbar0, s2barbar0)
   
    struct xbs {
	int m, tau;
	double sm, xdelta;
 
	xbs(int m, int tau=NA_INTEGER, double xdelta=0.0) :
	    m(m), tau(tau), sm(1/sqrt(static_cast<double>(m))),
	    xdelta(xdelta) {}

	double operator()(int i) {
	    boost::random::normal_distribution<double> normal;
	    sitmo::prng_engine *rng = threadRNG;
	    double u = normal(*rng);
	    if ((tau != NA_INTEGER) && (i>=(tau-1))) u = xdelta+u;
	    return u;
	}
    }; 
    
   struct xtbs {
    int m, tau;
    double sm, xdelta, nu;

    xtbs(int m, int tau=NA_INTEGER, double xdelta=0.0, double nu=4) :
        m(m), tau(tau), sm(1/sqrt(static_cast<double>(m))),
        xdelta(xdelta), nu(nu) {}

    double operator()(int i) {
        boost::random::student_t_distribution<double> tdist(nu);
        sitmo::prng_engine *rng = threadRNG;
        double u = tdist(*rng);
        if ((tau != NA_INTEGER) && (i>=(tau-1))) u = xdelta+u/std::sqrt(nu);
        return u;
    }
};

struct xgbs {
    int m, tau;
    double sm, xdelta, shape, scale;

    xgbs(int m, int tau=NA_INTEGER, double xdelta=0.0, double shape=0.5, double scale=1.0) :
        m(m), tau(tau), sm(1/sqrt(static_cast<double>(m))),
        xdelta(xdelta), shape(shape), scale(scale) {}

    double operator()(int i) {
        boost::random::gamma_distribution<double> gdist(shape, scale);
        sitmo::prng_engine *rng = threadRNG;
        double u = gdist(*rng);
        if ((tau != NA_INTEGER) && (i>=(tau-1))) u = xdelta+(u-shape*scale)/(std::sqrt(shape*scale*scale));
        return u;
    }
};

	
    // Self-learning limits
    struct sllimits {
	bool sl;
	int m, d;
	double Linf, Delta, A, B, dm, q, k, 
	     xbb,s2bb,muhat, s2hat, shat, lhat; 
	int N; 
//Normal Scores para datos agrupados con cuantil conocido o desconocido
std::vector<double> Normal_S(std::vector<double> muestra, double theta, double Ftheta) {
  int n = muestra.size();
  std::vector<double> rangos(n);
  std::vector<double> prob(n);
  std::vector<int> menores;
  std::vector<int> mayores;
  std::vector<double> Menores;
  std::vector<double> Mayores;
  for (int i = 0; i < n; i++) {
    if (muestra[i] <= theta) {
      menores.push_back(i);
       Menores.push_back(muestra[i]);
    } else {
      mayores.push_back(i);
      Mayores.push_back(muestra[i]);
    }
  }
  int n_men = menores.size();
  int n_may = mayores.size();
  
  if(n_men>0){   
  for (int i = 0; i < n_men; i++) {
    int index = menores[i];
    rangos[index] = (double) (count_if(Menores.begin(), Menores.end(), [=](double x){ return x < muestra[index]; }) + 1);
  }
}
if(n_may>0){
  for (int i = 0; i < n_may; i++) {
    int index = mayores[i];
    rangos[index] = (double) (count_if(Mayores.begin(), Mayores.end(), [=](double x){ return x < muestra[index]; }) + 1);
  }
}
if(n_men>0){
  for (int i = 0; i < n_men; i++) { 
    int index = menores[i];
    prob[index] = Ftheta * (rangos[index] - 0.5) / (double) (menores.size() );
  }
}
if(n_may>0){
  for (int i = 0; i < n_may; i++) {
    int index = mayores[i];
    prob[index] = Ftheta + (1 - Ftheta) * (rangos[index] - 0.5) / (double) (mayores.size() );
  }
}

  std::vector<double> Zs(n);
  for (int i = 0; i < n; i++) {
    Zs[i] = std::sqrt(2) * boost::math::erf_inv(2 * prob[i] - 1);
  }
  return Zs;
}
	sllimits(  double *lim,double xbb, double s2bb,int N) :
	    sl(::R_finite(lim[2])), m(std::floor(lim[4]+0.5)),
            Linf(lim[0]), Delta(lim[1]), A(lim[2]), B(lim[3]), dm(lim[4]),
            xbb(xbb), s2bb(s2bb) ,N(N){
	    setlim();
	} 
	void setlim() {
	    d = 0;
	    q = 0;
	    muhat = xbb; 
	    s2hat = s2bb;
	    shat = sqrt(s2bb);
	    lhat = Linf+Delta*sqrt(std::min(1.0,dm/(m)));
	}
	std::vector<double> update(std::vector<double> aux, std::vector<double> xis,double xi,std::vector<double> ref_sample,std::string est, double theta ,double Ftheta,bool SNS) {
	    if (sl) {
		// estimates update
		m=m+N; 
		// update the limits?
        d=d+N;
		q = q+(xi-muhat)*(xi-muhat)/s2hat;
		if (q<A*d-B) {
		if(SNS==true){
		if(est=="T"){
			 theta = quantile(aux,Ftheta);
		}
		ref_sample = Normal_S(aux,theta,Ftheta);	
		}
		else{	
			int M= aux.size();
			int M2=ref_sample.size();
			for(int j=M2; j<M2+M;j++){
			ref_sample.push_back(aux[j-M2]);	
			}	
		}
		xbb =media(ref_sample);
		s2bb = var(ref_sample,xbb);
		setlim();
		}
	    }
	    return ref_sample;
	}	
    }; // end sllimits
    struct Chart { 
	bool sim;
    int lstat;
	double *limit;
	int n_xi;
	Chart(bool sim, int lstat, double *limit) :
	    sim(sim), lstat(lstat), limit(limit) {}
        virtual ~Chart() {}
        virtual bool update(int i, double x, sllimits &sl, double *stat) = 0;	
int crl( std::vector<double>  ref_sample, xbs &sampler, int maxrl,int tao,double delta,int N,std::string dist,std::string est,double theta,double Ftheta,bool SNS) {
	int i=0;
    int n_xi;
    double stat[8];
    double m0=media(ref_sample);
    double s20=var(ref_sample,m0);
    sllimits sl(limit,m0,s20,N);
    std::vector<double>  xis(N);
    double xi;
    std::vector<double> aux;
    int M = ref_sample.size();
     for (i = 0; i <M; i++) {
     	aux.push_back(ref_sample[i]);
	 }
    for (i = 0; i < maxrl; i++) {
		if((tao-1)>i){ 
				for(int j=0; j<N; j++){
					xis[j] = sampler(i*N +j) ;
				}					 
	 	}
    	else{
					for(int j=0; j<N; j++){
					xis[j]=sampler(i*N +j)+delta ;
				}		
		}
		n_xi=xis.size();
		if(SNS==true){
			 xi = Seq_NS_l(xis,aux,theta,Ftheta);	
		}
		else{
			xi=0;
			for(int j=0; j<n_xi; j++){
				xi+=xis[j]/(std::sqrt(n_xi));
			}
		}
		for(int j=0; j<N ;j++){
			aux.push_back(xis[j]); 
		}
        if (update(i, xi, sl, stat)) break;
        ref_sample = sl.update(aux,xis,xi,ref_sample,est,theta,Ftheta,SNS);       
    }
    return i+1;
}
int crl_t( std::vector<double>  ref_sample, xtbs &sampler, int maxrl,int tao,double delta,int N,std::string dist,std::string est,double theta,double Ftheta,bool SNS) {
	int i=0;
    int n_xi;
    double stat[8];
    double m0=media(ref_sample);
    double s20=var(ref_sample,m0);
    sllimits sl(limit,m0,s20,N);
    std::vector<double>  xis(N);
    double xi;
    std::vector<double> aux;
    int M = ref_sample.size();
     for (i = 0; i <M; i++) {
     	aux.push_back(ref_sample[i]);
	 }
    for (i = 0; i < maxrl; i++) {
		if((tao-1)>i){ 
				for(int j=0; j<N; j++){
					xis[j] = sampler(i*N +j) ;
				}					 
	 	}
    	else{
					for(int j=0; j<N; j++){
					xis[j]=sampler(i*N +j)+delta ;
				}		
		}
		n_xi=xis.size();
		if(SNS==true){
			 xi = Seq_NS_l(xis,aux,theta,Ftheta);	
		}
		else{
			xi=0;
			for(int j=0; j<n_xi; j++){
				xi+=xis[j]/(std::sqrt(n_xi));
			}
		}
		for(int j=0; j<N ;j++){
			aux.push_back(xis[j]); 
		}
        if (update(i, xi, sl, stat)) break;
        ref_sample = sl.update(aux,xis,xi,ref_sample,est,theta,Ftheta,SNS);       
    }
    return i+1;
}
int crl_g( std::vector<double>  ref_sample, xgbs &sampler, int maxrl,int tao,double delta,int N,std::string dist,std::string est,double theta,double Ftheta,bool SNS) {
	int i=0;
    int n_xi;
    double stat[8];
    double m0=media(ref_sample);
    double s20=var(ref_sample,m0);
    sllimits sl(limit,m0,s20,N);
    std::vector<double>  xis(N);
    double xi;
    std::vector<double> aux;
    int M = ref_sample.size();
     for (i = 0; i <M; i++) {
     	aux.push_back(ref_sample[i]);
	 }
    for (i = 0; i < maxrl; i++) {
		if((tao-1)>i){ 
				for(int j=0; j<N; j++){
					xis[j] = sampler(i*N +j) ;
				}					 
	 	}
    	else{
					for(int j=0; j<N; j++){
					xis[j]=sampler(i*N +j)+delta ;
				}		
		}
		n_xi=xis.size();
		if(SNS==true){
			 xi = Seq_NS_l(xis,aux,theta,Ftheta);	
		}
		else{
			xi=0;
			for(int j=0; j<n_xi; j++){
				xi+=xis[j]/(std::sqrt(n_xi));
			}
		}
		for(int j=0; j<N ;j++){
			aux.push_back(xis[j]); 
		}
        if (update(i, xi, sl, stat)) break;
        ref_sample = sl.update(aux,xis,xi,ref_sample,est,theta,Ftheta,SNS);       
    }
    return i+1;
}
};
    struct ShewhartX : public Chart {
	ShewhartX(double *limit) :
	    Chart(::R_finite(limit[2]), 7, limit) {}
        bool update(int i, double x, sllimits &sl, double *stat) {
            stat[0] = x;
            stat[1] = sl.muhat;
            stat[2] = sl.muhat-sl.lhat*sl.shat;
            stat[3] = sl.muhat+sl.lhat*sl.shat;
            stat[4] = sl.muhat;
            stat[5] = sl.shat;
            stat[6] = sl.lhat;
            return (x<stat[2]) || (x>stat[3]);
        } 
    };
    struct EWMA : public Chart {
	double lambda, se;
	EWMA(double lambda, double *limit) :
	    Chart(true, 7, limit), lambda(lambda), se(sqrt(lambda/(2.0-lambda)))
        {}
 
        bool update(int i, double x, sllimits &sl, double *stat) {
            if (i==1) stat[0] = sl.muhat;
            stat[0] += lambda*(x-stat[0]);
            stat[1] = sl.muhat;
            stat[2] = sl.muhat-se*sl.lhat*sl.shat;
            stat[3] = sl.muhat+se*sl.lhat*sl.shat;
            stat[4] = sl.muhat;
            stat[5] = sl.shat;
            stat[6] = sl.lhat;
            return (stat[0]<stat[2]) || (stat[0]>stat[3]);
        } 
    };
    struct CUSUM : public Chart {
	double k;	
	CUSUM(double k, double *limit) :
	    Chart(true, 8, limit), k(k) {}
 
        bool update(int i, double x, sllimits &sl, double *stat) {
            if (i==1) stat[0] = stat[1] = stat[2] = 0;
            double r = (x-sl.muhat)/sl.shat;
            stat[0] = std::min(0.0, stat[0]+r+k);
            stat[1] = std::max(0.0, stat[1]+r-k);
            stat[3] = -sl.lhat;
            stat[4] = sl.lhat;
            stat[5] = sl.muhat;
            stat[6] = sl.shat;
            stat[7] = sl.lhat; 
            return (stat[0]<stat[3]) || (stat[1]>stat[4]);
        } 
    };   
    inline Chart &getChart(Rcpp::List chart) {
	Chart *c;
	std::string t=Rcpp::as<std::string>(chart["chart"]);
	Rcpp::NumericVector l=Rcpp::as<Rcpp::NumericVector>(chart["limit"]);
	if (t=="X") {
	    c = new ShewhartX(l.begin());
	} else if (t=="EWMA") {
	    c = new EWMA(Rcpp::as<double>(chart["lambda"]),l.begin());
	} else if (t=="CUSUM") {
	    c = new CUSUM(Rcpp::as<double>(chart["k"]),l.begin());
	} else {
	    Rcpp::stop("Unknown chart");
	} 
	return *c;
    }
    inline void simrl(Chart &c,xbs &s, std::vector<double> ref_sample, 
                      int nrl, int *rl, int maxrl,int tau,double delta,int N,std::string dist,std::string est, double theta,double Ftheta,bool SNS) {
#ifdef _OPENMP
        size_t nt = std::min(static_cast<uint32_t>(nrl/5),
                             parallel::rngs.nthreads);
#pragma omp parallel for num_threads(nt)
#endif
	for (int i=0; i<nrl; i++) {
	    rl[i] = c.crl(ref_sample,s,maxrl,tau,delta,N,dist,est,theta,Ftheta,SNS);
        }       
    } 
    inline void simrl_t(Chart &c,xtbs &s, std::vector<double> ref_sample, 
                      int nrl, int *rl, int maxrl,int tau,double delta,int N,std::string dist,std::string est, double theta,double Ftheta,bool SNS) {
#ifdef _OPENMP
        size_t nt = std::min(static_cast<uint32_t>(nrl/5),
                             parallel::rngs.nthreads);
#pragma omp parallel for num_threads(nt)
#endif
	for (int i=0; i<nrl; i++) {
	    rl[i] = c.crl_t(ref_sample,s,maxrl,tau,delta,N,dist,est,theta,Ftheta,SNS);
        }       
    } 
     inline void simrl_g(Chart &c,xgbs &s, std::vector<double> ref_sample, 
                      int nrl, int *rl, int maxrl,int tau,double delta,int N,std::string dist,std::string est, double theta,double Ftheta,bool SNS) {
#ifdef _OPENMP
        size_t nt = std::min(static_cast<uint32_t>(nrl/5),
                             parallel::rngs.nthreads);
#pragma omp parallel for num_threads(nt)
#endif
	for (int i=0; i<nrl; i++) {
	    rl[i] = c.crl_g(ref_sample,s,maxrl,tau,delta,N,dist,est,theta,Ftheta,SNS);
        }       
    }
}
// [[Rcpp::export]]
bool hasOMP() {
    bool ans; 
#ifdef _OPENMP 
    ans = true;
#else
    ans = false;
#endif
    return ans;
}
// [[Rcpp::export]]
void setOMPThreads(uint32_t nthreads) {
    parallel::rngs.setPThreads(nthreads);
}
// [[Rcpp::export]]
void setSITMOSeeds(double seed) {
    parallel::rngs.setPSeed(seed);
}
// [[Rcpp::export]]
Rcpp::IntegerVector rcrl2(std::vector<double> ref_sample,int n, Rcpp::List chart, 
                   int tau, double delta, int maxrl,int N,std::string dist,std::string est,double theta,double Ftheta,bool SNS,double t_df=4,double gam_shape=0.5,double gam_scale=1) {
    if (n<0) Rcpp::stop("n cannot be negative");
    Rcpp::IntegerVector rl(n);
    Chart &c=getChart(chart);
     int m = std::floor(c.limit[4]+0.5);
         if (est == "T"){
    	theta=quantile(ref_sample,Ftheta);
	}
    
     if(dist== "T"){
					xtbs s(m, tau, delta,t_df);
					simrl_t(c, s,ref_sample, n, rl.begin(), maxrl,tau,delta,N,dist,est,theta,Ftheta,SNS);
				} 
				else if (dist== "G"){
					xgbs s(m, tau, delta,gam_shape,gam_scale);
					simrl_g(c, s,ref_sample, n, rl.begin(), maxrl,tau,delta,N,dist,est,theta,Ftheta,SNS);
					}
				else{  
				xbs s(m, tau, delta);
				simrl(c, s,ref_sample, n, rl.begin(), maxrl,tau,delta,N,dist,est,theta,Ftheta,SNS);
					}
    delete &c;
    return rl;
} 


